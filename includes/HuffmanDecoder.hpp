#pragma once

#include "Huffman.hpp"
#include "BitReader.hpp"
#include <vector>
#include <algorithm>
#include <cstring>
#include <stdexcept>

class HuffmanDecoder
{
public:
	enum { FAST_BITS = 9 };
	enum { FAST_SIZE = 1 << FAST_BITS };
	static const uint32_t INVALID = 0xFFFFFFFF;

	// Build the lookup table from a canonical HuffmanTable
	void build(const HuffmanTable& table)
	{
		_maxLen = 0;
		for (auto l : table.lengths)
		{
			if (l > _maxLen)
				_maxLen = l;
		}

		// Initialize fast table to invalid
		for (int i = 0; i < FAST_SIZE; i++)
			_fast[i] = INVALID;
		_overflow.clear();

		for (size_t sym = 0; sym < table.codes.size(); sym++)
		{
			uint8_t len = table.lengths[sym];
			if (len == 0)
				continue;

			// Reverse the canonical code (MSB-first) to match the LSB-first bit reading
			uint16_t code = table.codes[sym];
			uint16_t reversed = reverseBits(code, len);

			if (len <= FAST_BITS)
			{
				// Fill all entries that share this reversed prefix.
				// The extra high bits can be anything, so I fill 2^(FAST_BITS - len) slots.
				int fillCount = 1 << (FAST_BITS - len);
				for (int i = 0; i < fillCount; i++)
				{
					uint32_t index = reversed | ((uint32_t)i << len);
					// Pack: symbol in low 16 bits, length in bits 16..23
					_fast[index] = (uint32_t)sym | ((uint32_t)len << 16);
				}
			}
			else
			{
				_overflow.push_back({reversed, len, (uint16_t)sym});
			}
		}

		// Sort overflow by length for efficient searching
		std::sort(_overflow.begin(), _overflow.end(),
			[](const OverflowEntry& a, const OverflowEntry& b)
			{
				return a.len < b.len;
			});
	}

	// Decode one symbol from the bit stream
	int decode(BitReader& br) const
	{
		// Peek up to FAST_BITS
		int peekCount = _maxLen < (int)FAST_BITS ? _maxLen : (int)FAST_BITS;
		uint32_t bits = br.peekBits(peekCount);

		uint32_t entry = _fast[bits & (FAST_SIZE - 1)];
		if (entry != INVALID)
		{
			int len = (int)(entry >> 16);
			int sym = (int)(entry & 0xFFFF);
			br.advance(len);
			return sym;
		}

		// Slow path: longer codes.
		// We already have `peekCount` bits in `bits`. Read more bit by bit.
		// Re-read from scratch for clarity
		uint32_t code = br.peekBits(_maxLen);

		for (const auto& ov : _overflow)
		{
			// Mask to ov.len bits
			uint32_t mask = (1u << ov.len) - 1;
			if ((code & mask) == ov.reversed)
			{
				br.advance(ov.len);
				return (int)ov.symbol;
			}
		}

		throw std::runtime_error("HuffmanDecoder: invalid Huffman code");
	}

private:
	struct OverflowEntry
	{
		uint16_t	reversed;	// bit-reversed code
		uint8_t		len;
		uint16_t	symbol;
	};

	uint32_t				_fast[FAST_SIZE];
	std::vector<OverflowEntry>	_overflow;
	int						_maxLen = 0;

	// Reverse `numBits` bits of `value`
	static uint16_t reverseBits(uint16_t value, int numBits)
	{
		uint16_t result = 0;
		for (int i = 0; i < numBits; i++)
		{
			result = (result << 1) | (value & 1);
			value >>= 1;
		}
		return result;
	}
};