#include "bettercompress/ResourceGuard.hpp"

namespace bettercompress {

namespace {

constexpr std::uint64_t MiB =
    1024ULL * 1024ULL;

constexpr std::uint64_t GiB =
    1024ULL * 1024ULL * 1024ULL;

ResourceLimits normalLimits() {

    ResourceLimits limits{};

    limits.maxInputSize =
        1ULL * GiB;

    limits.maxOutputSize =
        1ULL * GiB;

    limits.maxMemoryUsage =
        512ULL * MiB;

    limits.maxDictionaryEntries =
        256;

    limits.maxDictionaryEntrySize =
        1U * 1024U * 1024U;

    limits.maxTokenCount =
        1ULL * GiB;

    return limits;
}

ResourceLimits largeLimits() {

    ResourceLimits limits{};

    limits.maxInputSize =
        64ULL * GiB;

    limits.maxOutputSize =
        64ULL * GiB;

    limits.maxMemoryUsage =
        1024ULL * MiB;

    limits.maxDictionaryEntries =
        1024;

    limits.maxDictionaryEntrySize =
        1U * 1024U * 1024U;

    limits.maxTokenCount =
        64ULL * GiB;

    return limits;
}

}

ResourceGuard::ResourceGuard(ResourceMode mode) {

    if (mode == ResourceMode::Large) {
        limits_ = largeLimits();
    } else {
        limits_ = normalLimits();
    }
}

const ResourceLimits& ResourceGuard::limits() const {
    return limits_;
}

bool ResourceGuard::checkInputSize(
    std::uint64_t size
) const {

    return size <= limits_.maxInputSize;
}

bool ResourceGuard::checkOutputSize(
    std::uint64_t size
) const {

    return size <= limits_.maxOutputSize;
}

bool ResourceGuard::checkMemoryUsage(
    std::uint64_t size
) const {

    return size <= limits_.maxMemoryUsage;
}

bool ResourceGuard::checkDictionaryEntries(
    std::uint32_t count
) const {

    return count <= limits_.maxDictionaryEntries;
}

bool ResourceGuard::checkDictionaryEntrySize(
    std::uint32_t size
) const {

    return size <= limits_.maxDictionaryEntrySize;
}

bool ResourceGuard::checkTokenCount(
    std::uint64_t count
) const {

    return count <= limits_.maxTokenCount;
}

}
