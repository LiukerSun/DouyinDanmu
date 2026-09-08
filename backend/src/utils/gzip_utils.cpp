#include "utils/gzip_utils.h"
#include <zlib.h>
#include <cstring>

namespace douyin {

std::vector<uint8_t> GzipUtils::decompress(const uint8_t* data, size_t len) {
    if (!data || len == 0) return {};

    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));

    // 15 + 16 = automatic gzip detection
    if (inflateInit2(&zs, 15 + 16) != Z_OK) {
        return {};
    }

    zs.next_in = const_cast<Bytef*>(data);
    zs.avail_in = static_cast<uInt>(len);

    std::vector<uint8_t> output;
    output.reserve(len * 4); // Estimate decompressed size

    uint8_t buffer[32768];
    int ret;
    do {
        zs.next_out = buffer;
        zs.avail_out = sizeof(buffer);
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&zs);
            return {};
        }
        size_t have = sizeof(buffer) - zs.avail_out;
        output.insert(output.end(), buffer, buffer + have);
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
    return output;
}

std::vector<uint8_t> GzipUtils::decompress(const std::string& data) {
    return decompress(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::vector<uint8_t> GzipUtils::compress(const uint8_t* data, size_t len) {
    if (!data || len == 0) return {};

    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));

    // 15 + 16 for gzip format, compression level 6
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return {};
    }

    zs.next_in = const_cast<Bytef*>(data);
    zs.avail_in = static_cast<uInt>(len);

    std::vector<uint8_t> output;
    output.reserve(len);

    uint8_t buffer[32768];
    int ret;
    do {
        zs.next_out = buffer;
        zs.avail_out = sizeof(buffer);
        ret = deflate(&zs, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            return {};
        }
        size_t have = sizeof(buffer) - zs.avail_out;
        output.insert(output.end(), buffer, buffer + have);
    } while (ret != Z_STREAM_END);

    deflateEnd(&zs);
    return output;
}

} // namespace douyin
