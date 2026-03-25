#pragma once

#include <vector>
#include <cstdint>

// A token is either a literal byte or a (length, distance) back-reference.
// If distance == 0, it's a literal. Otherwise it's a match.
struct LZ77Token
{
	uint16_t	distance;
	uint16_t	length;
	uint8_t		literal;
};

class LZ77
{
public:
	static const int MAX_WINDOW   = 32768;
	static const int MAX_MATCH    = 258;
	static const int MIN_MATCH    = 3;

	static std::vector<LZ77Token> compress(const uint8_t* data, std::size_t size);
};