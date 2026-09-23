#include "bettercompress/Compressor.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bettercompress {

namespace {

constexpr std::size_t MIN_BUNDLE_LENGTH = 8;
constexpr std::size_t MAX_BUNDLE_LENGTH = 128;
constexpr std::size_t MAX_DICTIONARY_SIZE = 256;

struct Candidate {
    std::string text;
    std::size_t lineCount;
};

struct DictionaryEntry {
    std::string text;
    std::uint32_t id;
};

void writeUint16(std::ofstream& output, std::uint16_t value) {
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
}

void writeUint32(std::ofstream& output, std::uint32_t value) {
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
    output.put(static_cast<char>((value >> 16) & 0xFF));
    output.put(static_cast<char>((value >> 24) & 0xFF));
}

std::vector<std::pair<std::size_t, std::size_t>>
getLineRanges(const std::string& data) {

    std::vector<std::pair<std::size_t, std::size_t>> lines;

    std::size_t start = 0;

    for (std::size_t i = 0; i < data.size(); ++i) {

        if (data[i] == '\n') {
            lines.emplace_back(start, i);
            start = i + 1;
        }
    }

    if (start <= data.size()) {
        lines.emplace_back(start, data.size());
    }

    return lines;
}

} // namespace

bool compress(const std::string& inputPath,
              const std::string& outputPath) {

    std::ifstream input(inputPath, std::ios::binary);

    if (!input) {
        std::cerr << "Error: Could not open input file.\n";
        return false;
    }

    std::string data(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );

    if (data.empty()) {
        std::cerr << "Error: Input file is empty.\n";
        return false;
    }

    std::cout << "BetterCompress v0.21\n\n";

    // ------------------------------------------------------------
    // STEP 1: Find lines
    // ------------------------------------------------------------

    const auto lines = getLineRanges(data);

    std::cout << "Scanning "
              << lines.size()
              << " lines...\n";

    // ------------------------------------------------------------
    // STEP 2: Find bundles occurring across different lines
    // ------------------------------------------------------------

    std::unordered_map<std::string, std::size_t> lineCounts;
    std::unordered_map<std::string, std::size_t> lastLineSeen;

    for (std::size_t lineIndex = 0;
         lineIndex < lines.size();
         ++lineIndex) {

        const auto [lineStart, lineEnd] = lines[lineIndex];

        if (lineEnd <= lineStart) {
            continue;
        }

        const std::size_t lineLength =
            lineEnd - lineStart;

        std::unordered_set<std::string> bundlesThisLine;

        for (std::size_t position = 0;
             position < lineLength;
             ++position) {

            const std::size_t maxLength =
                std::min(
                    MAX_BUNDLE_LENGTH,
                    lineLength - position
                );

            for (std::size_t length = MIN_BUNDLE_LENGTH;
                 length <= maxLength;
                 ++length) {

                std::string bundle =
                    data.substr(
                        lineStart + position,
                        length
                    );

                if (bundle.find('\n') !=
                    std::string::npos) {
                    continue;
                }

                if (bundle.find('\r') !=
                    std::string::npos) {
                    continue;
                }

                bundlesThisLine.insert(
                    std::move(bundle)
                );
            }
        }

        for (const auto& bundle : bundlesThisLine) {

            auto lastIt = lastLineSeen.find(bundle);

            if (lastIt == lastLineSeen.end()) {

                lastLineSeen[bundle] = lineIndex;
                lineCounts[bundle] = 1;

            } else if (lastIt->second != lineIndex) {

                lastIt->second = lineIndex;
                ++lineCounts[bundle];
            }
        }
    }

    // ------------------------------------------------------------
    // STEP 3: Create candidates
    // ------------------------------------------------------------

    std::vector<Candidate> candidates;

    for (const auto& [text, count] : lineCounts) {

        if (count < 2) {
            continue;
        }

        candidates.push_back({
            text,
            count
        });
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& a, const Candidate& b) {

            const std::size_t scoreA =
                a.text.size() * a.lineCount;

            const std::size_t scoreB =
                b.text.size() * b.lineCount;

            if (scoreA != scoreB) {
                return scoreA > scoreB;
            }

            return a.text.size() > b.text.size();
        }
    );

    // ------------------------------------------------------------
    // STEP 4: Build dictionary
    // ------------------------------------------------------------

    std::vector<DictionaryEntry> dictionary;

    for (const Candidate& candidate : candidates) {

        if (dictionary.size() >=
            MAX_DICTIONARY_SIZE) {
            break;
        }

        bool redundant = false;

        for (const auto& entry : dictionary) {

            if (entry.text.size() <
                candidate.text.size()) {
                continue;
            }

            if (entry.text.find(candidate.text) !=
                std::string::npos) {

                redundant = true;
                break;
            }
        }

        if (redundant) {
            continue;
        }

        dictionary.push_back({
            candidate.text,
            static_cast<std::uint32_t>(
                dictionary.size()
            )
        });
    }

    // ------------------------------------------------------------
    // STEP 5: Encode using dictionary
    // ------------------------------------------------------------

    struct Token {
        bool dictionaryReference;
        std::uint32_t value;
        std::string literal;
    };

    std::vector<Token> tokens;

    std::size_t position = 0;

    while (position < data.size()) {

        std::size_t bestDictionary = 0;
        std::size_t bestLength = 0;

        for (std::size_t i = 0;
             i < dictionary.size();
             ++i) {

            const auto& text =
                dictionary[i].text;

            if (text.size() <= bestLength) {
                continue;
            }

            if (position + text.size() >
                data.size()) {
                continue;
            }

            if (data.compare(
                    position,
                    text.size(),
                    text
                ) == 0) {

                bestDictionary = i;
                bestLength = text.size();
            }
        }

        if (bestLength >= MIN_BUNDLE_LENGTH) {

            tokens.push_back({
                true,
                static_cast<std::uint32_t>(
                    bestDictionary
                ),
                {}
            });

            position += bestLength;
            continue;
        }

        // --------------------------------------------------------
        // Literal block
        // --------------------------------------------------------

        const std::size_t literalStart =
            position;

        ++position;

        while (position < data.size()) {

            bool matchFound = false;

            for (const auto& entry : dictionary) {

                if (entry.text.size() >
                    data.size() - position) {
                    continue;
                }

                if (data.compare(
                        position,
                        entry.text.size(),
                        entry.text
                    ) == 0) {

                    matchFound = true;
                    break;
                }
            }

            if (matchFound) {
                break;
            }

            ++position;
        }

        tokens.push_back({
            false,
            0,
            data.substr(
                literalStart,
                position - literalStart
            )
        });
    }

    // ------------------------------------------------------------
    // STEP 6: Determine dictionary ID width
    // ------------------------------------------------------------

    const std::uint8_t idWidth =
        dictionary.size() <= 255 ? 1 : 2;

    // ------------------------------------------------------------
    // STEP 7: Calculate actual compressed size
    // ------------------------------------------------------------

    std::size_t dictionarySize = 0;

    for (const auto& entry : dictionary) {

        dictionarySize += 4;
        dictionarySize += entry.text.size();
    }

    std::size_t tokenSize = 0;

    for (const auto& token : tokens) {

        if (token.dictionaryReference) {

            tokenSize += 1;
            tokenSize += idWidth;

        } else {

            tokenSize += 1;
            tokenSize += 4;
            tokenSize += token.literal.size();
        }
    }

    const std::size_t compressedSize =
        4 +
        1 +
        1 +
        4 +
        dictionarySize +
        tokenSize;

    const std::size_t rawSize =
        4 +
        1 +
        4 +
        data.size();

    // ------------------------------------------------------------
    // STEP 8: Fall back to raw storage if necessary
    // ------------------------------------------------------------

    if (dictionary.empty() ||
        compressedSize >= rawSize) {

        std::cout
            << "Dictionary compression was not beneficial.\n"
            << "Stored file without compression.\n";

        std::ofstream output(
            outputPath,
            std::ios::binary
        );

        if (!output) {
            std::cerr
                << "Error: Could not create output file.\n";
            return false;
        }

        output.write("BC21", 4);

        output.put(0);

        writeUint32(
            output,
            static_cast<std::uint32_t>(
                data.size()
            )
        );

        output.write(
            data.data(),
            static_cast<std::streamsize>(
                data.size()
            )
        );

        return true;
    }

    // ------------------------------------------------------------
    // STEP 9: Write compressed file
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

    output.write("BC21", 4);

    output.put(1); // dictionary mode

    output.put(
        static_cast<char>(idWidth)
    );

    writeUint32(
        output,
        static_cast<std::uint32_t>(
            dictionary.size()
        )
    );

    for (const auto& entry : dictionary) {

        writeUint32(
            output,
            static_cast<std::uint32_t>(
                entry.text.size()
            )
        );

        output.write(
            entry.text.data(),
            static_cast<std::streamsize>(
                entry.text.size()
            )
        );
    }

    for (const auto& token : tokens) {

        if (token.dictionaryReference) {

            output.put(1);

            if (idWidth == 1) {

                output.put(
                    static_cast<char>(
                        token.value
                    )
                );

            } else {

                writeUint16(
                    output,
                    static_cast<std::uint16_t>(
                        token.value
                    )
                );
            }

        } else {

            output.put(0);

            writeUint32(
                output,
                static_cast<std::uint32_t>(
                    token.literal.size()
                )
            );

            output.write(
                token.literal.data(),
                static_cast<std::streamsize>(
                    token.literal.size()
                )
            );
        }
    }

    std::cout
        << "Compression successful.\n"
        << "Dictionary entries: "
        << dictionary.size()
        << '\n'
        << "Original size: "
        << data.size()
        << " bytes\n"
        << "Compressed size: "
        << compressedSize
        << " bytes\n";

    return true;
}

} // namespace bettercompress