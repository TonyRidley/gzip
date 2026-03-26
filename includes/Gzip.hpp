#pragma once

#include <vector>
#include <cstdint>

class Gzip
{
public:
	// Wrap data in a gzip container (RFC 1952)
	static std::vector<uint8_t> compress(const uint8_t* data, std::size_t size);

	// Unwrap a gzip container and decompress
	static std::vector<uint8_t> decompress(const uint8_t* data, std::size_t size);

private:
	// CRC-32 (ISO 3309 / ITU-T V.42, same polynomial as gzip)
	static uint32_t crc32(const uint8_t* data, std::size_t size);
};