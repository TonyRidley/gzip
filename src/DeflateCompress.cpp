#include "Deflate.hpp"
#include "DeflateTables.hpp"
#include "LZ77.hpp"
#include "Huffman.hpp"
#include "BitWriter.hpp"
#include <vector>
#include <cstdint>

std::vector<uint8_t> Deflate::compress(const uint8_t* data, std::size_t size)
{
    BitWriter bw;

    // LZ77
    auto tokens = LZ77::compress(data, size);

    // Build frequency tables
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

    // Trim trailing zeros
    int hlit = 286;
    while (hlit > 257 && litLengths[hlit - 1] == 0)
        hlit--;
    int hdist = 30;
    while (hdist > 1 && distLengths[hdist - 1] == 0)
        hdist--;

    auto litTable  = Huffman::buildCanonicalTable(litLengths);
    auto distTable = Huffman::buildCanonicalTable(distLengths);

    // Combine lit + dist code lengths, then RLE-encode
    std::vector<uint8_t> allLengths;
    for (int i = 0; i < hlit; i++)
        allLengths.push_back(litLengths[i]);
    for (int i = 0; i < hdist; i++)
        allLengths.push_back(distLengths[i]);

    std::vector<uint8_t>  clSymbols;
    std::vector<uint8_t>  clExtra;
    std::vector<int>      clExtraBits;

    for (size_t i = 0; i < allLengths.size(); )
    {
        uint8_t val = allLengths[i];
        if (val == 0)
        {
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
            clSymbols.push_back(val);
            clExtra.push_back(0);
            clExtraBits.push_back(0);
            i++;
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

    // Huffman table for code-length alphabet (0-18)
    std::vector<uint32_t> clFreqs(19, 0);
    for (auto s : clSymbols)
        clFreqs[s]++;
    auto clLens = Huffman::buildCodeLengths(clFreqs, 7);
    auto clTable = Huffman::buildCanonicalTable(clLens);

    int hclen = 19;
    while (hclen > 4 && clLens[clOrder[hclen - 1]] == 0)
        hclen--;

    // Write block header
    bw.writeBits(1, 1); // BFINAL = 1
    bw.writeBits(2, 2); // BTYPE  = 2 (dynamic Huffman)

    bw.writeBits(hlit - 257, 5);
    bw.writeBits(hdist - 1, 5);
    bw.writeBits(hclen - 4, 4);

    for (int i = 0; i < hclen; i++)
        bw.writeBits(clLens[clOrder[i]], 3);

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
            bw.writeBitsMSB(litTable.codes[tok.literal],
                            litTable.lengths[tok.literal]);
        }
        else
        {
            int lc = lengthToCode(tok.length);
            bw.writeBitsMSB(litTable.codes[lc], litTable.lengths[lc]);
            int li = lc - 257;
            if (lengthExtraBits[li] > 0)
                bw.writeBits(tok.length - lengthBase[li], lengthExtraBits[li]);

            int dc = distToCode(tok.distance);
            bw.writeBitsMSB(distTable.codes[dc], distTable.lengths[dc]);
            if (distExtraBits[dc] > 0)
                bw.writeBits(tok.distance - distBase[dc], distExtraBits[dc]);
        }
    }

    // End-of-block
    bw.writeBitsMSB(litTable.codes[256], litTable.lengths[256]);
    bw.flushByte();

    return bw.bytes();
}