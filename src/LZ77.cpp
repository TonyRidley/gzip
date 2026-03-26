#include "LZ77.hpp"
#include <algorithm>
#include <cstring>

// Multiplicative hash, better distribution than simple byte packing.
// Reduces collisions, which means shorter chains and faster matching.
static uint32_t hash3(const uint8_t* p)
{
    return ((uint32_t)p[0] * 0x1E35A7BDu ^
            (uint32_t)p[1] * 0x344B7235u ^
            (uint32_t)p[2] * 0x638E14D7u);
}

// Find the best match at `pos` using hash chains.
// Returns length (0 if no match ≥ MIN_MATCH found), and sets bestDist.
static int findBestMatch(const uint8_t* data, std::size_t size,
                         std::size_t pos,
                         const std::vector<int>& head,
                         const std::vector<int>& prev,
                         int hashSize,
                         int& bestDist)
{
    static const int MAX_CHAIN    = 64;
    static const int GOOD_ENOUGH  = 32;  // stop searching if we find a match this long

    if (pos + LZ77::MIN_MATCH > size)
        return 0;

    uint32_t h = hash3(data + pos) & (hashSize - 1);
    int matchPos = head[h];
    int bestLen = 0;
    bestDist = 0;

    std::size_t windowStart = (pos > (std::size_t)LZ77::MAX_WINDOW) ? pos - LZ77::MAX_WINDOW : 0;
    int maxLen = (int)std::min((std::size_t)LZ77::MAX_MATCH, size - pos);
    int chainLen = 0;

    while (matchPos >= (int)windowStart && chainLen < MAX_CHAIN)
    {
        // Quick check: compare first byte and the byte just past the current best
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
                if (bestLen >= LZ77::MAX_MATCH || bestLen >= GOOD_ENOUGH)
                    break;
            }
        }
        matchPos = prev[matchPos];
        chainLen++;
    }

    return bestLen;
}

// Insert position `pos` into the hash chain
static void insertHash(const uint8_t* data, std::size_t size,
                               std::size_t pos,
                               std::vector<int>& head,
                               std::vector<int>& prev,
                               int hashSize)
{
    if (pos + LZ77::MIN_MATCH <= size)
    {
        uint32_t h = hash3(data + pos) & (hashSize - 1);
        prev[pos] = head[h];
        head[h] = (int)pos;
    }
}

std::vector<LZ77Token> LZ77::compress(const uint8_t* data, std::size_t size)
{
    std::vector<LZ77Token> tokens;
    tokens.reserve(size);

    static const int HASH_SIZE = 1 << 18;   // 256K buckets

    std::vector<int> head(HASH_SIZE, -1);
    std::vector<int> prev(size, -1);

    std::size_t pos = 0;

    // State for lazy matching: if we found a match at `pos`, we check
    // whether `pos+1` gives a *better* match. If so, emit `pos` as a
    // literal and use the longer match instead.
    int  pendingLen  = 0;
    int  pendingDist = 0;

    while (pos < size)
    {
        int curLen = 0;
        int curDist = 0;

        curLen = findBestMatch(data, size, pos, head, prev, HASH_SIZE, curDist);

        if (pendingLen > 0)
        {
            // We have a pending match from the previous position.
            // Lazy evaluation: is the current match strictly better?
            if (curLen > pendingLen)
            {
                // Current match is better, emit the pending position as a literal,
                // and carry this match forward as the new pending match.
                LZ77Token litTok;
                litTok.distance = 0;
                litTok.length   = 0;
                litTok.literal  = data[pos - 1];
                tokens.push_back(litTok);

                // The previous position was already inserted into the hash.
                // Now this position becomes the new pending match.
                insertHash(data, size, pos, head, prev, HASH_SIZE);
                pendingLen  = curLen;
                pendingDist = curDist;
                pos++;
            }
            else
            {
                // Pending match is at least as good — emit it.
                LZ77Token matchTok;
                matchTok.distance = (uint16_t)pendingDist;
                matchTok.length   = (uint16_t)pendingLen;
                matchTok.literal  = 0;
                tokens.push_back(matchTok);

                // Insert all the skipped positions into the hash chains
                // (pos-1 was already inserted; insert pos through pos-1+pendingLen-1)
                std::size_t matchEnd = (pos - 1) + (std::size_t)pendingLen;
                for (std::size_t i = pos; i < matchEnd && i + MIN_MATCH <= size; i++)
                    insertHash(data, size, i, head, prev, HASH_SIZE);

                pos = matchEnd;
                pendingLen = 0;
                pendingDist = 0;
            }
        }
        else if (curLen >= MIN_MATCH)
        {
            // Found a match. Don't emit yet. Store as pending and check pos+1.
            insertHash(data, size, pos, head, prev, HASH_SIZE);
            pendingLen  = curLen;
            pendingDist = curDist;
            pos++;
        }
        else
        {
            // No match. Emit literal
            insertHash(data, size, pos, head, prev, HASH_SIZE);

            LZ77Token litTok;
            litTok.distance = 0;
            litTok.length   = 0;
            litTok.literal  = data[pos];
            tokens.push_back(litTok);
            pos++;
        }
    }

    // Flush any remaining pending match
    if (pendingLen > 0)
    {
        LZ77Token matchTok;
        matchTok.distance = (uint16_t)pendingDist;
        matchTok.length   = (uint16_t)pendingLen;
        matchTok.literal  = 0;
        tokens.push_back(matchTok);
    }

    return tokens;
}