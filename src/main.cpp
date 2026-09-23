#include "bettercompress/Compressor.hpp"
#include "bettercompress/Decompressor.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {

    if (argc != 4) {
        std::cout
            << "BetterCompress v0.1\n\n"
            << "Usage:\n"
            << "  bettercompress compress <input> <output>\n"
            << "  bettercompress decompress <input> <output>\n";

        return 1;
    }

    const std::string command = argv[1];
    const std::string input = argv[2];
    const std::string output = argv[3];

    if (command == "compress") {
        return bettercompress::compress(input, output) ? 0 : 1;
    }

    if (command == "decompress") {
        return bettercompress::decompress(input, output) ? 0 : 1;
    }

    std::cerr << "Error: Unknown command: "
              << command << '\n';

    return 1;
}