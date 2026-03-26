#include "Gzip.hpp"
#include "Deflate.hpp"
#include <stdexcept>
#include <cstring>

// CRC-32
static uint32_t crc32Table[256];
static bool crc32TableBuilt = false;

static void buildCRC32Table()
{
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
        crc32Table[i] = crc;
    }
    crc32TableBuilt = true;
}

uint32_t Gzip::crc32(const uint8_t* data, std::size_t size)
{
    if (!crc32TableBuilt)
        buildCRC32Table();

    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; i++)
        crc = crc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// Write a little-endian 16/32-bit value

static void writeLE32(std::vector<uint8_t>& out, uint32_t val)
{
    out.push_back(val & 0xFF);
    out.push_back((val >> 8) & 0xFF);
    out.push_back((val >> 16) & 0xFF);
    out.push_back((val >> 24) & 0xFF);
}

static uint16_t readLE16(const uint8_t* p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t readLE32(const uint8_t* p)
{
    return (uint32_t)p[0]       | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Compress
std::vector<uint8_t> Gzip::compress(const uint8_t* data, std::size_t size)
{
    std::vector<uint8_t> out;

    // Gzip header RFC 1952
    out.push_back(0x1F);       // ID1
    out.push_back(0x8B);       // ID2
    out.push_back(0x08);       // CM = deflate
    out.push_back(0x00);       // FLG = 0 (no extras)
    writeLE32(out, 0);    // MTIME = 0
    out.push_back(0x00);       // XFL
    out.push_back(0xFF);       // OS = unknown

    // Compressed data
    auto deflated = Deflate::compress(data, size);
    out.insert(out.end(), deflated.begin(), deflated.end());

    // Gzip trailer
    writeLE32(out, crc32(data, size));          // CRC32
    writeLE32(out, (uint32_t)(size & 0xFFFFFFFFu)); // ISIZE (mod 2^32)

    return out;
}

// Decompress
std::vector<uint8_t> Gzip::decompress(const uint8_t* data, std::size_t size)
{
    if (size < 18)
        throw std::runtime_error("Gzip: input too short");

    // Parse header
    if (data[0] != 0x1F || data[1] != 0x8B)
        throw std::runtime_error("Gzip: bad magic number");
    if (data[2] != 0x08)
        throw std::runtime_error("Gzip: unsupported compression method");

    uint8_t flg = data[3];
    std::size_t pos = 10;

    // FEXTRA
    if (flg & 0x04)
    {
        if (pos + 2 > size)
            throw std::runtime_error("Gzip: truncated FEXTRA");
        uint16_t xlen = readLE16(data + pos);
        pos += 2 + xlen;
    }
    // FNAME
    if (flg & 0x08)
    {
        while (pos < size && data[pos] != 0) pos++;
        pos++;
    }
    // FCOMMENT
    if (flg & 0x10)
    {
        while (pos < size && data[pos] != 0) pos++;
        pos++;
    }
    // FHCRC
    if (flg & 0x02)
        pos += 2;

    if (pos >= size)
        throw std::runtime_error("Gzip: truncated header");

    // The last 8 bytes are the trailer (CRC32 + ISIZE)
    if (size - pos < 8)
        throw std::runtime_error("Gzip: truncated trailer");

    std::size_t compressedSize = size - pos - 8;
    const uint8_t* compressedData = data + pos;

    uint32_t expectedCRC  = readLE32(data + size - 8);
    uint32_t expectedSize = readLE32(data + size - 4);

    // Decompress
    auto output = Deflate::decompress(compressedData, compressedSize);

    // Verify
    uint32_t actualCRC = crc32(output.data(), output.size());
    if (actualCRC != expectedCRC)
        throw std::runtime_error("Gzip: CRC32 mismatch");
    if ((uint32_t)(output.size() & 0xFFFFFFFFu) != expectedSize)
        throw std::runtime_error("Gzip: size mismatch");

    return output;
}