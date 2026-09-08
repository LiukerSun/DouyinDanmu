#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace douyin {

class GzipUtils {
public:
    // Decompress gzip data. Returns empty vector on failure.
    static std::vector<uint8_t> decompress(const uint8_t* data, size_t len);

    // Convenience overload for std::string (binary data)
    static std::vector<uint8_t> decompress(const std::string& data);

    // Compress data with gzip. Returns empty vector on failure.
    static std::vector<uint8_t> compress(const uint8_t* data, size_t len);
};

} // namespace douyin
