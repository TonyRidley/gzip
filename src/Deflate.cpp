#include "Deflate.hpp"
#include "LZ77.hpp"
#include "Huffman.hpp"
#include "BitWriter.hpp"
#include "BitReader.hpp"
#include <algorithm>
#include <stdexcept>
#include <cstring>

// RFC 1951 tables for length/distance coding

// Length codes 257..285
static const int lengthBase[] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
    35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const int lengthExtraBits[] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
    3,3,3,3,4,4,4,4,5,5,5,5,0
};

// Distance codes 0..29
static const int distBase[] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
    257,385,513,769,1025,1537,2049,3073,4097,6145,
    8193,12289,16385,24577
};
static const int distExtraBits[] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
    7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

static int lengthToCode(int length)
{
    for (int i = 28; i >= 0; i--)
    {
        if (length >= lengthBase[i])
            return 257 + i;
    }
    return 257;
}

static int distToCode(int dist)
{
    for (int i = 29; i >= 0; i--)
    {
        if (dist >= distBase[i])
            return i;
    }
    return 0;
}

// Code length alphabet ordering (RFC 1951 §3.2.7)
static const int clOrder[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

// Compress
std::vector<uint8_t> Deflate::compress(const uint8_t* data, std::size_t size)
{
    BitWriter bw;

    // LZ77
    auto tokens = LZ77::compress(data, size);

    // Build frequency tables
    // Literal/length alphabet: 0..285 (0-255 = literals, 256 = end, 257-285 = lengths)
    std::vector<uint32_t> litFreqs(286, 0);
    std::vector<uint32_t> distFreqs(30, 0);

    for (auto& tok : tokens)
    {
        if (tok.distance == 0)
        {
            litFreqs[tok.literal]++;
        }
        else
        {
            int lc = lengthToCode(tok.length);
            litFreqs[lc]++;
            int dc = distToCode(tok.distance);
            distFreqs[dc]++;
        }
    }
    litFreqs[256] = 1;

    // Build Huffman tables
    auto litLengths  = Huffman::buildCodeLengths(litFreqs, 15);
    auto distLengths = Huffman::buildCodeLengths(distFreqs, 15);

    // Ensure at least 1 distance code exists
    // HLIT must encode at least up to 257; HDIST at least 1 symbol
    int hlit = 286;
    while (hlit > 257 && litLengths[hlit - 1] == 0)
        hlit--;
    int hdist = 30;
    while (hdist > 1 && distLengths[hdist - 1] == 0)
        hdist--;

    auto litTable  = Huffman::buildCanonicalTable(litLengths);
    auto distTable = Huffman::buildCanonicalTable(distLengths);

    // Encode the code lengths themselves using RLE
    // Combine lit + dist code lengths into one sequence
    std::vector<uint8_t> allLengths;
    for (int i = 0; i < hlit; i++)
        allLengths.push_back(litLengths[i]);
    for (int i = 0; i < hdist; i++)
        allLengths.push_back(distLengths[i]);

    // RLE encode: symbols 0-15 literal, 16=repeat prev, 17=repeat 0 (3-10), 18=repeat 0 (11-138)
    std::vector<uint8_t>  clSymbols;
    std::vector<uint8_t>  clExtra;
    std::vector<int>      clExtraBits;

    for (size_t i = 0; i < allLengths.size(); )
    {
        uint8_t val = allLengths[i];
        if (val == 0)
        {
            // Count consecutive zeros
            int run = 1;
            while (i + run < allLengths.size() && allLengths[i + run] == 0 && run < 138)
                run++;
            if (run >= 11)
            {
                clSymbols.push_back(18);
                clExtra.push_back(run - 11);
                clExtraBits.push_back(7);
                i += run;
            }
            else if (run >= 3)
            {
                clSymbols.push_back(17);
                clExtra.push_back(run - 3);
                clExtraBits.push_back(3);
                i += run;
            }
            else
            {
                clSymbols.push_back(0);
                clExtra.push_back(0);
                clExtraBits.push_back(0);
                i++;
            }
        }
        else
        {
            // Emit the value itself
            clSymbols.push_back(val);
            clExtra.push_back(0);
            clExtraBits.push_back(0);
            i++;
            // Then check for repeats
            int run = 0;
            while (i + run < allLengths.size() && allLengths[i + run] == val && run < 6)
                run++;
            if (run >= 3)
            {
                clSymbols.push_back(16);
                clExtra.push_back(run - 3);
                clExtraBits.push_back(2);
                i += run;
            }
        }
    }

    // Build Huffman table for the code-length alphabet (0-18)
    std::vector<uint32_t> clFreqs(19, 0);
    for (auto s : clSymbols)
        clFreqs[s]++;
    auto clLens = Huffman::buildCodeLengths(clFreqs, 7);
    auto clTable = Huffman::buildCanonicalTable(clLens);

    // Determine HCLEN
    int hclen = 19;
    while (hclen > 4 && clLens[clOrder[hclen - 1]] == 0)
        hclen--;

    // Write block header
    bw.writeBits(1, 1); // BFINAL = 1 (last block)
    bw.writeBits(2, 2); // BTYPE  = 2 (dynamic Huffman)

    bw.writeBits(hlit - 257, 5);
    bw.writeBits(hdist - 1, 5);
    bw.writeBits(hclen - 4, 4);

    // Write code-length code lengths in the special order
    for (int i = 0; i < hclen; i++)
        bw.writeBits(clLens[clOrder[i]], 3);

    // Write the RLE-encoded literal/distance code lengths
    for (size_t i = 0; i < clSymbols.size(); i++)
    {
        uint8_t sym = clSymbols[i];
        bw.writeBitsMSB(clTable.codes[sym], clTable.lengths[sym]);
        if (clExtraBits[i] > 0)
            bw.writeBits(clExtra[i], clExtraBits[i]);
    }

    // Write compressed data
    for (auto& tok : tokens)
    {
        if (tok.distance == 0)
        {
            // Literal
            bw.writeBitsMSB(litTable.codes[tok.literal],
                            litTable.lengths[tok.literal]);
        }
        else
        {
            // Length
            int lc = lengthToCode(tok.length);
            bw.writeBitsMSB(litTable.codes[lc], litTable.lengths[lc]);
            int li = lc - 257;
            if (lengthExtraBits[li] > 0)
                bw.writeBits(tok.length - lengthBase[li], lengthExtraBits[li]);

            // Distance
            int dc = distToCode(tok.distance);
            bw.writeBitsMSB(distTable.codes[dc], distTable.lengths[dc]);
            if (distExtraBits[dc] > 0)
                bw.writeBits(tok.distance - distBase[dc], distExtraBits[dc]);
        }
    }

    // End-of-block (symbol 256)
    bw.writeBitsMSB(litTable.codes[256], litTable.lengths[256]);
    bw.flushByte();

    return bw.bytes();
}

// Decompress

// Build a decoding lookup: given a BitReader and a canonical table, decode one symbol
static int decodeSymbol(BitReader& br, const HuffmanTable& table)
{
    uint8_t maxLen = *std::max_element(table.lengths.begin(), table.lengths.end());
    uint16_t code = 0;

    for (int len = 1; len <= maxLen; len++)
    {
        code <<= 1;
        code |= br.readBits(1);
        // Search for a matching code at this length
        for (size_t sym = 0; sym < table.codes.size(); sym++)
        {
            if (table.lengths[sym] == len && table.codes[sym] == code)
                return (int)sym;
        }
    }
    throw std::runtime_error("Deflate: invalid Huffman code");
}

std::vector<uint8_t> Deflate::decompress(const uint8_t* data, std::size_t size)
{
    BitReader br(data, size);
    std::vector<uint8_t> output;

    bool bfinal = false;
    while (!bfinal)
    {
        bfinal = br.readBits(1) != 0;
        uint32_t btype = br.readBits(2);

        if (btype == 0)
        {
            // No compression
            br.alignToByte();
            uint16_t len  = (uint16_t)br.readBits(16);
            uint16_t nlen = (uint16_t)br.readBits(16);
            if ((uint16_t)(len ^ 0xFFFF) != nlen)
                throw std::runtime_error("Deflate: stored block length mismatch");
            for (uint16_t i = 0; i < len; i++)
                output.push_back((uint8_t)br.readBits(8));
        }
        else if (btype == 1 || btype == 2)
        {
            HuffmanTable litTable, distTable;

            if (btype == 1)
            {
                // Fixed Huffman codes (RFC 1951 §3.2.6)
                std::vector<uint8_t> litLens(288, 0);
                for (int i = 0; i <= 143; i++)   litLens[i] = 8;
                for (int i = 144; i <= 255; i++) litLens[i] = 9;
                for (int i = 256; i <= 279; i++) litLens[i] = 7;
                for (int i = 280; i <= 287; i++) litLens[i] = 8;
                litTable = Huffman::buildCanonicalTable(litLens);

                std::vector<uint8_t> dLens(30, 5);
                distTable = Huffman::buildCanonicalTable(dLens);
            }
            else
            {
                // Dynamic Huffman codes
                int hlit  = br.readBits(5) + 257;
                int hdist = br.readBits(5) + 1;
                int hclen = br.readBits(4) + 4;

                // Read code-length code lengths
                std::vector<uint8_t> clLens(19, 0);
                for (int i = 0; i < hclen; i++)
                    clLens[clOrder[i]] = (uint8_t)br.readBits(3);

                auto clTable = Huffman::buildCanonicalTable(clLens);

                // Decode literal/distance code lengths
                std::vector<uint8_t> allLengths;
                int total = hlit + hdist;
                while ((int)allLengths.size() < total)
                {
                    int sym = decodeSymbol(br, clTable);
                    if (sym <= 15)
                    {
                        allLengths.push_back((uint8_t)sym);
                    }
                    else if (sym == 16)
                    {
                        int repeat = br.readBits(2) + 3;
                        uint8_t prev = allLengths.back();
                        for (int r = 0; r < repeat; r++)
                            allLengths.push_back(prev);
                    }
                    else if (sym == 17)
                    {
                        int repeat = br.readBits(3) + 3;
                        for (int r = 0; r < repeat; r++)
                            allLengths.push_back(0);
                    }
                    else if (sym == 18)
                    {
                        int repeat = br.readBits(7) + 11;
                        for (int r = 0; r < repeat; r++)
                            allLengths.push_back(0);
                    }
                }

                std::vector<uint8_t> litLens(allLengths.begin(), allLengths.begin() + hlit);
                std::vector<uint8_t> dLens(allLengths.begin() + hlit, allLengths.begin() + hlit + hdist);

                // Pad to full sizes
                litLens.resize(286, 0);
                dLens.resize(30, 0);

                litTable  = Huffman::buildCanonicalTable(litLens);
                distTable = Huffman::buildCanonicalTable(dLens);
            }

            // Decode symbols
            while (true)
            {
                int sym = decodeSymbol(br, litTable);
                if (sym == 256)
                    break; // end of block
                if (sym < 256)
                {
                    output.push_back((uint8_t)sym);
                }
                else
                {
                    // Length
                    int li = sym - 257;
                    int length = lengthBase[li];
                    if (lengthExtraBits[li] > 0)
                        length += br.readBits(lengthExtraBits[li]);

                    // Distance
                    int dsym = decodeSymbol(br, distTable);
                    int distance = distBase[dsym];
                    if (distExtraBits[dsym] > 0)
                        distance += br.readBits(distExtraBits[dsym]);

                    // Copy from output buffer
                    size_t srcPos = output.size() - distance;
                    for (int j = 0; j < length; j++)
                        output.push_back(output[srcPos + j]);
                }
            }
        }
        else
        {
            throw std::runtime_error("Deflate: invalid block type");
        }
    }

    return output;
}