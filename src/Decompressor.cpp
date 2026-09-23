#include "bettercompress/Decompressor.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace bettercompress {

namespace {

bool readUint16(std::ifstream& input, std::uint16_t& value) {
    const int b0 = input.get();
    const int b1 = input.get();

    if (b0 == EOF || b1 == EOF) {
        return false;
    }

    value = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(b0) |
        (static_cast<std::uint16_t>(b1) << 8)
    );

    return true;
}

bool readUint32(std::ifstream& input, std::uint32_t& value) {
    const int b0 = input.get();
    const int b1 = input.get();
    const int b2 = input.get();
    const int b3 = input.get();

    if (b0 == EOF ||
        b1 == EOF ||
        b2 == EOF ||
        b3 == EOF) {
        return false;
    }

    value =
        static_cast<std::uint32_t>(b0) |
        (static_cast<std::uint32_t>(b1) << 8) |
        (static_cast<std::uint32_t>(b2) << 16) |
        (static_cast<std::uint32_t>(b3) << 24);

    return true;
}

} // namespace

bool decompress(const std::string& inputPath,
                const std::string& outputPath) {

    std::ifstream input(
        inputPath,
        std::ios::binary
    );

    if (!input) {
        std::cerr
            << "Error: Could not open compressed file.\n";
        return false;
    }

    // ------------------------------------------------------------
    // STEP 1: Check file signature
    // ------------------------------------------------------------

    char magic[4];

    input.read(magic, 4);

    if (input.gcount() != 4 ||
        magic[0] != 'B' ||
        magic[1] != 'C' ||
        magic[2] != '2' ||
        magic[3] != '1') {

        std::cerr
            << "Error: Not a BetterCompress v0.21 file.\n";

        return false;
    }

    // ------------------------------------------------------------
    // STEP 2: Read compression mode
    // ------------------------------------------------------------

    const int mode = input.get();

    if (mode == EOF) {
        std::cerr
            << "Error: Corrupted BetterCompress file.\n";
        return false;
    }

    // ------------------------------------------------------------
    // RAW MODE
    // ------------------------------------------------------------

    if (mode == 0) {

        std::uint32_t originalSize = 0;

        if (!readUint32(input, originalSize)) {
            std::cerr
                << "Error: Invalid raw file header.\n";
            return false;
        }

        std::string data(originalSize, '\0');

        input.read(
            data.data(),
            static_cast<std::streamsize>(
                originalSize
            )
        );

        if (input.gcount() !=
            static_cast<std::streamsize>(
                originalSize
            )) {

            std::cerr
                << "Error: Raw data is incomplete.\n";

            return false;
        }

        std::ofstream output(
            outputPath,
            std::ios::binary
        );

        if (!output) {
            std::cerr
                << "Error: Could not create output file.\n";
            return false;
        }

        output.write(
            data.data(),
            static_cast<std::streamsize>(
                data.size()
            )
        );

        std::cout
            << "File was stored without compression.\n"
            << "Restored "
            << data.size()
            << " bytes.\n";

        return true;
    }

    // ------------------------------------------------------------
    // DICTIONARY MODE
    // ------------------------------------------------------------

    if (mode != 1) {

        std::cerr
            << "Error: Unknown compression mode.\n";

        return false;
    }

    // ------------------------------------------------------------
    // STEP 3: Read dictionary ID width
    // ------------------------------------------------------------

    const int idWidth = input.get();

    if (idWidth != 1 && idWidth != 2) {

        std::cerr
            << "Error: Invalid dictionary ID width.\n";

        return false;
    }

    // ------------------------------------------------------------
    // STEP 4: Read dictionary count
    // ------------------------------------------------------------

    std::uint32_t dictionaryCount = 0;

    if (!readUint32(input, dictionaryCount)) {

        std::cerr
            << "Error: Could not read dictionary count.\n";

        return false;
    }

    if (dictionaryCount == 0 ||
        dictionaryCount > 1024) {

        std::cerr
            << "Error: Invalid dictionary size.\n";

        return false;
    }

    std::vector<std::string> dictionary;

    dictionary.reserve(dictionaryCount);

    // ------------------------------------------------------------
    // STEP 5: Read dictionary
    // ------------------------------------------------------------

    for (std::uint32_t i = 0;
         i < dictionaryCount;
         ++i) {

        std::uint32_t length = 0;

        if (!readUint32(input, length)) {

            std::cerr
                << "Error: Could not read dictionary entry.\n";

            return false;
        }

        if (length > 1024 * 1024) {

            std::cerr
                << "Error: Dictionary entry is too large.\n";

            return false;
        }

        std::string entry(length, '\0');

        input.read(
            entry.data(),
            static_cast<std::streamsize>(
                length
            )
        );

        if (input.gcount() !=
            static_cast<std::streamsize>(
                length
            )) {

            std::cerr
                << "Error: Dictionary entry is incomplete.\n";

            return false;
        }

        dictionary.push_back(
            std::move(entry)
        );
    }

    // ------------------------------------------------------------
    // STEP 6: Decode tokens
    // ------------------------------------------------------------

    std::string restored;

    while (true) {

        const int tokenType = input.get();

        if (tokenType == EOF) {
            break;
        }

        // --------------------------------------------------------
        // Dictionary reference
        // --------------------------------------------------------

        if (tokenType == 1) {

            std::uint32_t dictionaryID = 0;

            if (idWidth == 1) {

                const int value = input.get();

                if (value == EOF) {

                    std::cerr
                        << "Error: Incomplete dictionary reference.\n";

                    return false;
                }

                dictionaryID =
                    static_cast<std::uint32_t>(
                        value
                    );

            } else {

                std::uint16_t value = 0;

                if (!readUint16(input, value)) {

                    std::cerr
                        << "Error: Incomplete dictionary reference.\n";

                    return false;
                }

                dictionaryID =
                    static_cast<std::uint32_t>(
                        value
                    );
            }

            if (dictionaryID >=
                dictionary.size()) {

                std::cerr
                    << "Error: Invalid dictionary reference.\n";

                return false;
            }

            restored +=
                dictionary[dictionaryID];

        }

        // --------------------------------------------------------
        // Literal block
        // --------------------------------------------------------

        else if (tokenType == 0) {

            std::uint32_t length = 0;

            if (!readUint32(input, length)) {

                std::cerr
                    << "Error: Invalid literal block.\n";

                return false;
            }

            if (length > 1024 * 1024 * 1024) {

                std::cerr
                    << "Error: Literal block is too large.\n";

                return false;
            }

            std::string literal(length, '\0');

            input.read(
                literal.data(),
                static_cast<std::streamsize>(
                    length
                )
            );

            if (input.gcount() !=
                static_cast<std::streamsize>(
                    length
                )) {

                std::cerr
                    << "Error: Literal block is incomplete.\n";

                return false;
            }

            restored += literal;
        }

        else {

            std::cerr
                << "Error: Unknown token type.\n";

            return false;
        }
    }

    // ------------------------------------------------------------
    // STEP 7: Write restored file
    // ------------------------------------------------------------

    std::ofstream output(
        outputPath,
        std::ios::binary
    );

    if (!output) {

        std::cerr
            << "Error: Could not create output file.\n";

        return false;
    }

    output.write(
        restored.data(),
        static_cast<std::streamsize>(
            restored.size()
        )
    );

    std::cout
        << "Decompression successful.\n"
        << "Dictionary entries: "
        << dictionary.size()
        << '\n'
        << "Restored size: "
        << restored.size()
        << " bytes\n";

    return true;
}

} // namespace bettercompress