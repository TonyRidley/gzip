#include "Deflate.hpp"
#include "DeflateTables.hpp"
#include "Huffman.hpp"
#include "BitReader.hpp"
#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>

static int decodeSymbol(BitReader& br, const HuffmanTable& table)
{
    uint8_t maxLen = *std::max_element(table.lengths.begin(), table.lengths.end());
    uint16_t code = 0;

    for (int len = 1; len <= maxLen; len++)
    {
        code <<= 1;
        code |= br.readBits(1);
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

                std::vector<uint8_t> clLens(19, 0);
                for (int i = 0; i < hclen; i++)
                    clLens[clOrder[i]] = (uint8_t)br.readBits(3);

                auto clTable = Huffman::buildCanonicalTable(clLens);

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
                    break;
                if (sym < 256)
                {
                    output.push_back((uint8_t)sym);
                }
                else
                {
                    int li = sym - 257;
                    int length = lengthBase[li];
                    if (lengthExtraBits[li] > 0)
                        length += br.readBits(lengthExtraBits[li]);

                    int dsym = decodeSymbol(br, distTable);
                    int distance = distBase[dsym];
                    if (distExtraBits[dsym] > 0)
                        distance += br.readBits(distExtraBits[dsym]);

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