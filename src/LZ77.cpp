#include "LZ77.hpp"
#include <algorithm>
#include <cstring>
#include <unordered_map>

std::vector<LZ77Token> LZ77::compress(const uint8_t* data, std::size_t size)
{
    std::vector<LZ77Token> tokens;
    tokens.reserve(size);

    // Hash table: maps a 3-byte sequence to the list of positions where it occurs.
    // I use the most recent positions to limit chain length.
    // Key = 3-byte hash, Value = list of positions
    auto hash3 = [](const uint8_t* p) -> uint32_t {
        return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    };

    // For each 3-byte hash, store a chain of positions
    static const int HASH_SIZE = 1 << 18;   // 256K buckets
    static const int MAX_CHAIN = 64;        // max positions to check per hash

    // head[h] = most recent position with this hash, prev[pos] = previous position with same hash
    std::vector<int> head(HASH_SIZE, -1);
    std::vector<int> prev(size, -1);

    std::size_t pos = 0;

    while (pos < size)
    {
        int bestLen = 0;
        int bestDist = 0;

        if (pos + MIN_MATCH <= size)
        {
            uint32_t h = hash3(data + pos) & (HASH_SIZE - 1);
            int chainLen = 0;
            int matchPos = head[h];

            std::size_t windowStart = (pos > (std::size_t)MAX_WINDOW) ? pos - MAX_WINDOW : 0;
            int maxLen = (int)std::min((std::size_t)MAX_MATCH, size - pos);

            while (matchPos >= (int)windowStart && chainLen < MAX_CHAIN)
            {
                // Quick check: compare first and last bytes before full scan
                if (data[matchPos] == data[pos] &&
                    data[matchPos + bestLen] == data[pos + bestLen])
                {
                    int len = 0;
                    while (len < maxLen && data[matchPos + len] == data[pos + len])
                        len++;
                    if (len > bestLen)
                    {
                        bestLen = len;
                        bestDist = (int)(pos - matchPos);
                        if (bestLen == MAX_MATCH)
                            break;
                    }
                }
                matchPos = prev[matchPos];
                chainLen++;
            }

            // Insert current position into the hash chain
            prev[pos] = head[h];
            head[h] = (int)pos;
        }

        if (bestLen >= MIN_MATCH)
        {
            LZ77Token tok;
            tok.distance = (uint16_t)bestDist;
            tok.length   = (uint16_t)bestLen;
            tok.literal  = 0;
            tokens.push_back(tok);

            // Insert skipped positions into the hash chains too,
            // so future matches can find them
            for (std::size_t i = pos + 1; i < pos + bestLen && i + MIN_MATCH <= size; i++)
            {
                uint32_t h2 = hash3(data + i) & (HASH_SIZE - 1);
                prev[i] = head[h2];
                head[h2] = (int)i;
            }

            pos += bestLen;
        }
        else
        {
            LZ77Token tok;
            tok.distance = 0;
            tok.length   = 0;
            tok.literal  = data[pos];
            tokens.push_back(tok);
            pos++;
        }
    }

    return tokens;
}