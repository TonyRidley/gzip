#pragma once

#include <vector>
#include <cstdint>

class BitWriter
{
private:
	std::vector<uint8_t>	_buffer;
	int						_bitPos = 0;

public:
	void writeBits(uint32_t value, int numBits)
	{
		int	i;
		i = 0;

		while (i < numBits)
		{
			if (_bitPos == 0)
				this->_buffer.push_back(0);
			if (value & (1u << i))
				this->_buffer.back() |= (1u << _bitPos);
			_bitPos = (_bitPos + 1) & 7;
			i++;
		}
	}

	void writeBitsMSB(uint32_t value, int numBits)
	{
		int	i;
		i = numBits - 1;
		while (i >= 0)
		{
			if (_bitPos == 0)
				this->_buffer.push_back(0);
			if (value & (1u << i))
				this->_buffer.back() |= (1u << _bitPos);
			_bitPos = (_bitPos + 1) & 7;
			--i;
		}
	}

	void flushByte()
	{
		if (_bitPos != 0)
			_bitPos = 0;
	}

	const std::vector<uint8_t>& bytes() const { return _buffer; }

	std::size_t bitCount() const
	{
		return _buffer.size() * 8 - (_bitPos == 0 ? 0 : 8 - _bitPos);
	}
};

