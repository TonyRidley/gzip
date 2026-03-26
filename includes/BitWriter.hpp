#pragma once

#include <vector>
#include <cstdint>

class BitWriter
{
private:
	std::vector<uint8_t>	_buffer;
	uint64_t				_accumulator = 0;
	int						_bitCount = 0;

	// Flush complete bytes from the accumulator into the buffer
	void drain()
	{
		while (_bitCount >= 8)
		{
			_buffer.push_back((uint8_t)(_accumulator & 0xFF));
			_accumulator >>= 8;
			_bitCount -= 8;
		}
	}

public:
	// Write `numBits` bits from `value` (LSB-first, as deflate expects for extra bits)
	void writeBits(uint32_t value, int numBits)
	{
		_accumulator |= ((uint64_t)value << _bitCount);
		_bitCount += numBits;
		drain();
	}

	// Write `numBits` bits from `value` MSB-first (for Huffman codes).
	// Reverses the bits so they go into the LSB-first bitstream correctly.
	void writeBitsMSB(uint32_t value, int numBits)
	{
		// Reverse the bits of value
		uint32_t reversed = 0;
		for (int i = 0; i < numBits; i++)
		{
			reversed = (reversed << 1) | (value & 1);
			value >>= 1;
		}
		writeBits(reversed, numBits);
	}

	void flushByte()
	{
		if (_bitCount > 0)
		{
			_buffer.push_back((uint8_t)(_accumulator & 0xFF));
			_accumulator = 0;
			_bitCount = 0;
		}
	}

	const std::vector<uint8_t>& bytes() const { return _buffer; }

	std::size_t bitCount() const
	{
		return _buffer.size() * 8 + _bitCount;
	}
};