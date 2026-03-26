#pragma once

#include <cstdint>
#include <vector>

struct HuffmanTable
{
	std::vector<uint16_t>		codes;    // canonical Huffman codes
	std::vector<std::uint8_t>	lengths;  // bit lengths per symbol
};

class Huffman
{
public:
	// Given symbol frequencies, build optimal code lengths
	static std::vector<uint8_t> buildCodeLengths(const std::vector<uint32_t>& freqs, uint8_t maxBits);

	// Given code lengths, generate canonical Huffman codes (RFC 1951 algorithm)
	static HuffmanTable buildCanonicalTable(const std::vector<uint8_t>& lengths);

private:
	struct Node
	{
		uint32_t freq;
		int      symbol;
		int      left;
		int      right;
	};

	// Walk tree to extract code lengths per symbol
	static void extractLengths(const std::vector<Node>& nodes, int nodeIdx,
							   uint8_t depth, std::vector<uint8_t>& lengths);

	// If any code exceeds maxBits, flatten the lengths to fit
	static void clampLengths(std::vector<uint8_t>& lengths, uint8_t maxBits);
};
