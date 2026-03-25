#include "LZ77.hpp"
#include <cstring>

// Hash table size — must be a power of 2
static const int HASH_BITS = 15;
static const int HASH_SIZE = 1 << HASH_BITS;  // 32768
static const int HASH_MASK = HASH_SIZE - 1;

// Max candidates to check per position before giving up.
static const int MAX_CHAIN = 128;

// Hash 3 bytes into a table index
static int hashThree(const uint8_t* p)
{
	return ((p[0] << 10) ^ (p[1] << 5) ^ p[2]) & HASH_MASK;
}

std::vector<LZ77Token> LZ77::compress(const uint8_t* data, size_t size)
{
	std::vector<LZ77Token> tokens;

	if (size == 0)
		return tokens;

	// hashHead[h] = most recent position where hash == h, or -1 if none
	std::vector<int> hashHead(HASH_SIZE, -1);

	// hashPrev[pos % MAX_WINDOW] = previous position with the same hash.
	// Forms a linked list per hash bucket, newest → oldest.
	std::vector<int> hashPrev(MAX_WINDOW, -1);

	size_t pos = 0;

	while (pos < size)
	{
		int bestLen = 0;
		int bestDist = 0;

		// We need at least 3 bytes to compute a hash and to have a valid match
		if (pos + 2 < size)
		{
			int h = hashThree(data + pos);

			// Walk the chain for this hash bucket
			int candidate = hashHead[h];
			int chainLen = 0;

			while (candidate != -1 && chainLen < MAX_CHAIN)
			{
				int dist = (int)(pos - candidate);

				// Too far back: Everything further in the chain is even older
				if (dist > MAX_WINDOW)
					break;

				// Compare bytes vs current position
				int len = 0;
				while (len < MAX_MATCH && pos + len < size && data[candidate + len] == data[pos + len])
				{
					++len;
				}

				if (len > bestLen)
				{
					bestLen = len;
					bestDist = dist;
					if (len == MAX_MATCH)
						break; // can't do better
				}

				// Follow the chain to the next candidate
				candidate = hashPrev[candidate % MAX_WINDOW];
				++chainLen;
			}

			// Insert current position into the hash chain
			hashPrev[pos % MAX_WINDOW] = hashHead[h];
			hashHead[h] = (int)pos;
		}

		if (bestLen >= MIN_MATCH)
		{
			LZ77Token tok;
			tok.distance = (uint16_t)bestDist;
			tok.length   = (uint16_t)bestLen;
			tok.literal  = 0;
			tokens.push_back(tok);

			// Insert the skipped positions into the hash table too,
			// so future matches can find them
			for (size_t i = 1; i < (size_t)bestLen; ++i)
			{
				size_t p = pos + i;
				if (p + 2 < size)
				{
					int h = hashThree(data + p);
					hashPrev[p % MAX_WINDOW] = hashHead[h];
					hashHead[h] = (int)p;
				}
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
			++pos;
		}
	}

	return tokens;
}