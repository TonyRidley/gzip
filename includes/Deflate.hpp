#pragma once

#include <vector>
#include <cstdint>

class Deflate
{
public:
	// Compress raw data into a DEFLATE stream
	static std::vector<uint8_t> compress(const uint8_t* data, std::size_t size);

	// Decompress a DEFLATE stream back to raw data
	static std::vector<uint8_t> decompress(const uint8_t* data, std::size_t size);
};