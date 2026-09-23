#pragma once

#include <string>

namespace bettercompress {

bool decompress(const std::string& inputPath,
                const std::string& outputPath);

}