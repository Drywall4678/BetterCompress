#pragma once

#include <cstddef>
#include <cstdint>

namespace bettercompress {

enum class ResourceMode {
    Normal,
    Large
};

struct ResourceLimits {
    std::uint64_t maxInputSize;
    std::uint64_t maxOutputSize;
    std::uint64_t maxMemoryUsage;

    std::uint32_t maxDictionaryEntries;
    std::uint32_t maxDictionaryEntrySize;

    std::uint64_t maxTokenCount;
};

class ResourceGuard {
public:
    explicit ResourceGuard(ResourceMode mode = ResourceMode::Normal);

    const ResourceLimits& limits() const;

    bool checkInputSize(std::uint64_t size) const;
    bool checkOutputSize(std::uint64_t size) const;
    bool checkMemoryUsage(std::uint64_t size) const;

    bool checkDictionaryEntries(
        std::uint32_t count
    ) const;

    bool checkDictionaryEntrySize(
        std::uint32_t size
    ) const;

    bool checkTokenCount(
        std::uint64_t count
    ) const;

private:
    ResourceLimits limits_;
};

}
