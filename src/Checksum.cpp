#include "bettercompress/Checksum.hpp"

#include <array>

namespace bettercompress {

std::uint32_t crc32(const std::string& data) {

    static const std::array<std::uint32_t, 256> table = [] {

        std::array<std::uint32_t, 256> result{};

        for (std::uint32_t i = 0; i < 256; ++i) {

            std::uint32_t value = i;

            for (int bit = 0; bit < 8; ++bit) {

                if (value & 1U) {
                    value =
                        (value >> 1) ^
                        0xEDB88320U;
                } else {
                    value >>= 1;
                }
            }

            result[i] = value;
        }

        return result;
    }();

    std::uint32_t checksum = 0xFFFFFFFFU;

    for (unsigned char byte : data) {

        const std::uint8_t index =
            static_cast<std::uint8_t>(
                (checksum ^ byte) & 0xFFU
            );

        checksum =
            (checksum >> 8) ^
            table[index];
    }

    return checksum ^ 0xFFFFFFFFU;
}

}