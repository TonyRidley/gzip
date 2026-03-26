#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>

class BitReader
{
private:
	const uint8_t*	_data;
	size_t			_bytePos = 0;
	size_t			_size = 0;
	uint64_t		_accumulator = 0;
	int				_bitsInAcc = 0;   // number of valid bits in the accumulator

	// Refill the accumulator so it has at least 32 bits (or as many as remain)
	void refill()
	{
		while (_bitsInAcc <= 56 && _bytePos < _size)
		{
			_accumulator |= ((uint64_t)_data[_bytePos]) << _bitsInAcc;
			_bytePos++;
			_bitsInAcc += 8;
		}
	}

public:
	BitReader(const uint8_t* data, size_t size) : _data(data), _size(size)
	{
		refill();
	}

	// Peek up to 25 bits without consuming them (LSB-first)
	uint32_t peekBits(int numBits) const
	{
		return (uint32_t)(_accumulator & ((1ULL << numBits) - 1));
	}

	// Consume `numBits` bits (call after peekBits)
	void advance(int numBits)
	{
		_accumulator >>= numBits;
		_bitsInAcc -= numBits;
		refill();
	}

	// Read `numBits` bits LSB-first and consume them
	uint32_t readBits(int numBits)
	{
		if (_bitsInAcc < numBits)
			refill();
		if (_bitsInAcc < numBits)
			throw std::runtime_error("BitReader: Read past end of data.");
		uint32_t result = (uint32_t)(_accumulator & ((1ULL << numBits) - 1));
		_accumulator >>= numBits;
		_bitsInAcc -= numBits;
		refill();
		return result;
	}

	// Read `numBits` bits MSB-first (for Huffman codes written MSB-first)
	uint32_t readBitsMSB(int numBits)
	{
		// Read LSB-first, then reverse
		uint32_t raw = readBits(numBits);
		uint32_t result = 0;
		for (int i = 0; i < numBits; i++)
		{
			result = (result << 1) | (raw & 1);
			raw >>= 1;
		}
		return result;
	}

	void alignToByte()
	{
		// Discard bits until at a byte boundary.
		// The accumulator holds _bitsInAcc bits; need to drop the sub-byte remainder.
		int discard = _bitsInAcc & 7;  // bits past the last byte boundary
		if (discard > 0)
		{
			_accumulator >>= discard;
			_bitsInAcc -= discard;
		}
	}

	bool hasMore() const { return _bitsInAcc > 0 || _bytePos < _size; }
	size_t bytePosition() const { return _bytePos; }
	int bitPosition() const { return 0; }  // sub-byte position is tracked in the accumulator
};