# gzip

A small C++ implementation of gzip compression and decompression. The project builds a command-line program named `gzip` that can wrap raw input in a gzip container, compress it with DEFLATE, and restore gzip files back to their original bytes.

This is a learning-focused implementation of the gzip format, LZ77 matching, canonical Huffman coding, and DEFLATE bitstreams. It is useful for understanding how gzip works internally rather than as a replacement for the system `gzip` utility.

## Features

- Compress files into `.gz` output
- Decompress gzip files back to raw data
- Gzip header and trailer handling according to RFC 1952
- CRC-32 and uncompressed-size validation during decompression
- LZ77 sliding-window match finder
- Dynamic Huffman block generation for compression
- Stored, fixed-Huffman, and dynamic-Huffman block support for decompression
- Round-trip test script for text, binary, random, and compatibility cases

## Requirements

- `make`
- A C++ compiler available as `c++`
- `bash` for the test script
- `python3`, `xxd`, and `gunzip` for the full test suite

No third-party libraries are required.

## Building

```bash
make
```

This creates the executable:

```bash
./gzip
```

Clean build artifacts with:

```bash
make clean
```

Remove the executable as well:

```bash
make fclean
```

Rebuild from scratch:

```bash
make re
```

## Usage

Use `./gzip` so the project executable is used instead of your system gzip command.

Compress a file:

```bash
./gzip compress input.txt input.txt.gz
```

Decompress a file:

```bash
./gzip decompress input.txt.gz restored.txt
```

The program prints the number of bytes read and written. If the input is invalid, decompression fails with an error such as a bad magic number, unsupported compression method, CRC mismatch, or size mismatch.

## Testing

Build first, then run:

```bash
make
./test.sh
```

The test script checks:

- empty files
- single-byte files
- repeated text
- all 256 byte values
- larger text files
- random binary data
- compression of a project source file
- gzip magic bytes
- compatibility with the system `gunzip`

## Project Structure

```text
.
├── Makefile
├── README.md
├── test.sh
├── includes/
│   ├── BitReader.hpp
│   ├── BitWriter.hpp
│   ├── Deflate.hpp
│   ├── DeflateTables.hpp
│   ├── Gzip.hpp
│   ├── Huffman.hpp
│   ├── HuffmanDecoder.hpp
│   └── LZ77.hpp
└── src/
    ├── DeflateCompress.cpp
    ├── DeflateDecompress.cpp
    ├── Gzip.cpp
    ├── Huffman.cpp
    ├── LZ77.cpp
    └── main.cpp
```

## How It Works

Compression starts in `main.cpp`, which reads the input file and calls `Gzip::compress`. `Gzip.cpp` writes the gzip header, calls the DEFLATE compressor, then appends the CRC-32 and original input size.

The DEFLATE compressor uses `LZ77.cpp` to convert input bytes into literal and back-reference tokens. It then builds Huffman code lengths from token frequencies, creates canonical Huffman tables, writes a dynamic-Huffman DEFLATE block, and emits the compressed bitstream through `BitWriter`.

Decompression reverses the process. `Gzip::decompress` parses the gzip container, extracts the DEFLATE payload, verifies the trailer, and writes the restored bytes. The DEFLATE decompressor supports stored blocks, fixed Huffman blocks, and dynamic Huffman blocks.

## Notes and Limitations

- The compressor writes a single final DEFLATE block using dynamic Huffman codes.
- The decompressor handles multiple DEFLATE blocks.
- The implementation reads each input file into memory before processing it.
- The executable is named `gzip`, so run it as `./gzip` to avoid calling the system command.
- This project is intended for learning and coursework, not production archival use.

## Sources Used

- https://www.infinitepartitions.com/art001.html
- https://en.wikipedia.org/wiki/Gzip#File_format
- https://en.wikipedia.org/wiki/LZ77_and_LZ78#LZ77
- https://en.wikipedia.org/wiki/Huffman_coding
- https://www.ietf.org/rfc/rfc1951.txt
- https://www.ietf.org/rfc/rfc1952.txt
