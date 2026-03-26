#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

#include "Gzip.hpp"

static std::vector<uint8_t> readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Cannot open file: " + path);
    }
    return {std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()};
}

static void writeFile(const std::string& path, const std::vector<uint8_t>& data)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Cannot write file: " + path);
    }
    file.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
}

static void printUsage(const char* progName)
{
    std::cout << "Usage:\n"
              << "  " << progName << " compress   <input_file> <output_file.gz>\n"
              << "  " << progName << " decompress <input_file.gz> <output_file>\n";
}

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    std::string inputPath = argv[2];
    std::string outputPath = argv[3];

    try
    {
        auto input = readFile(inputPath);

        if (mode == "compress")
        {
            std::cout << "Compressing " << input.size() << " bytes...\n";

            auto compressed = Gzip::compress(input.data(), input.size());

            writeFile(outputPath, compressed);
            std::cout << "Written " << compressed.size() << " bytes to " << outputPath << "\n";
        }
        else if (mode == "decompress")
        {
            std::cout << "Decompressing " << input.size() << " bytes...\n";

            auto decompressed = Gzip::decompress(input.data(), input.size());

            writeFile(outputPath, decompressed);
            std::cout << "Written " << decompressed.size() << " bytes to " << outputPath << "\n";
        }
        else
        {
            std::cerr << "Unknown mode: " << mode << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}