#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>

class BitReader
{
private:
	const uint8_t*			_data;
	size_t					_bitPos = 0;
	size_t					_bytePos = 0;
	size_t					_size = 0;

public:
	BitReader(const uint8_t* data, size_t size) : _data(data), _size(size) {}

	uint32_t readBits(int numBits)
	{
		uint32_t	result = 0;
		int			i;

		i = 0;

		while (i < numBits)
		{
			if (_bitPos >= _size)
			{
				throw std::runtime_error("BitReader: Read past end of data.");
			}

			if (_data[_bytePos] & (1u << _bitPos))
				result |= (1u << i);

			_bitPos++;
			if (_bitPos == 8)
			{
				_bitPos = 0;
				_bytePos++;
			}
			i++;
		}
		return result;
	}

	uint32_t readBitsMSB(int numBits)
	{
		uint32_t	result = 0;
		int			i;

		i = 0;

		while (i < numBits)
		{
			if (_bitPos >= _size)
			{
				throw std::runtime_error("BitReader: Read past end of data.");
			}

			result <<= 1;
			if (_data[_bytePos] & (1u << _bitPos))
				result |= 1;

			_bitPos++;
			if (_bitPos == 8)
			{
				_bitPos = 0;
				_bytePos++;
			}
			i++;
		}
		return result;
	}

	void alignToByte()
	{
		if (_bitPos != 0)
		{
			_bitPos = 0;
			_bytePos++;
		}
	}

	bool hasMore() const { return _bytePos < _size; }
	size_t bytePosition() const { return _bytePos; }
	int bitPosition() const { return _bitPos; }
};

