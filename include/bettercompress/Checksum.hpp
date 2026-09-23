#pragma once

#include <cstdint>
#include <string>

namespace bettercompress {

std::uint32_t crc32(const std::string& data);

}