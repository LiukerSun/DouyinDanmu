#pragma once

// Standalone MD5 implementation (RFC 1321)
// No external dependencies required.

#include <cstdint>
#include <cstring>
#include <string>

namespace douyin {
namespace md5 {

class MD5 {
public:
    static constexpr size_t DIGEST_LENGTH = 16;

    MD5() { reset(); }

    void reset() {
        m_state[0] = 0x67452301;
        m_state[1] = 0xEFCDAB89;
        m_state[2] = 0x98BADCFE;
        m_state[3] = 0x10325476;
        m_count = 0;
        std::memset(m_buffer, 0, BLOCK_SIZE);
    }

    void update(const uint8_t* data, size_t len) {
        size_t offset = m_count % BLOCK_SIZE;
        m_count += len;

        // Process any buffered data first
        if (offset > 0) {
            size_t available = BLOCK_SIZE - offset;
            if (len >= available) {
                std::memcpy(m_buffer + offset, data, available);
                transform(m_buffer);
                data += available;
                len -= available;
                offset = 0;
            } else {
                std::memcpy(m_buffer + offset, data, len);
                return;
            }
        }

        // Process full blocks
        while (len >= BLOCK_SIZE) {
            transform(data);
            data += BLOCK_SIZE;
            len -= BLOCK_SIZE;
        }

        // Buffer remaining data
        if (len > 0) {
            std::memcpy(m_buffer + offset, data, len);
        }
    }

    void finalize(uint8_t digest[DIGEST_LENGTH]) {
        // Pad the message
        size_t offset = m_count % BLOCK_SIZE;
        size_t pad_len = (offset < 56) ? (56 - offset) : (120 - offset);

        uint8_t padding[64];
        std::memset(padding, 0, pad_len);
        padding[0] = 0x80;
        update(padding, pad_len);

        // Append length in bits (little-endian)
        uint64_t bits = m_count * 8;
        uint8_t length_bytes[8];
        for (int i = 0; i < 8; ++i) {
            length_bytes[i] = static_cast<uint8_t>(bits >> (i * 8));
        }
        update(length_bytes, 8);

        // Output digest
        for (int i = 0; i < 4; ++i) {
            digest[i * 4 + 0] = static_cast<uint8_t>(m_state[i]);
            digest[i * 4 + 1] = static_cast<uint8_t>(m_state[i] >> 8);
            digest[i * 4 + 2] = static_cast<uint8_t>(m_state[i] >> 16);
            digest[i * 4 + 3] = static_cast<uint8_t>(m_state[i] >> 24);
        }
    }

    // Convenience: hash a string and return hex digest
    static std::string hash_hex(const std::string& input) {
        MD5 ctx;
        ctx.update(reinterpret_cast<const uint8_t*>(input.data()), input.size());
        uint8_t digest[DIGEST_LENGTH];
        ctx.finalize(digest);

        static const char hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(DIGEST_LENGTH * 2);
        for (size_t i = 0; i < DIGEST_LENGTH; ++i) {
            result += hex[(digest[i] >> 4) & 0x0F];
            result += hex[digest[i] & 0x0F];
        }
        return result;
    }

    // Convenience: hash raw bytes and return digest
    static void hash(const uint8_t* data, size_t len, uint8_t digest[DIGEST_LENGTH]) {
        MD5 ctx;
        ctx.update(data, len);
        ctx.finalize(digest);
    }

private:
    static constexpr size_t BLOCK_SIZE = 64;

    void transform(const uint8_t block[BLOCK_SIZE]) {
        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        uint32_t M[16];

        for (int i = 0; i < 16; ++i) {
            M[i] = static_cast<uint32_t>(block[i * 4])
                 | (static_cast<uint32_t>(block[i * 4 + 1]) << 8)
                 | (static_cast<uint32_t>(block[i * 4 + 2]) << 16)
                 | (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
        }

        // Round 1
        FF(a, b, c, d, M[ 0],  7, 0xD76AA478); FF(d, a, b, c, M[ 1], 12, 0xE8C7B756);
        FF(c, d, a, b, M[ 2], 17, 0x242070DB); FF(b, c, d, a, M[ 3], 22, 0xC1BDCEEE);
        FF(a, b, c, d, M[ 4],  7, 0xF57C0FAF); FF(d, a, b, c, M[ 5], 12, 0x4787C62A);
        FF(c, d, a, b, M[ 6], 17, 0xA8304613); FF(b, c, d, a, M[ 7], 22, 0xFD469501);
        FF(a, b, c, d, M[ 8],  7, 0x698098D8); FF(d, a, b, c, M[ 9], 12, 0x8B44F7AF);
        FF(c, d, a, b, M[10], 17, 0xFFFF5BB1); FF(b, c, d, a, M[11], 22, 0x895CD7BE);
        FF(a, b, c, d, M[12],  7, 0x6B901122); FF(d, a, b, c, M[13], 12, 0xFD987193);
        FF(c, d, a, b, M[14], 17, 0xA679438E); FF(b, c, d, a, M[15], 22, 0x49B40821);

        // Round 2
        GG(a, b, c, d, M[ 1],  5, 0xF61E2562); GG(d, a, b, c, M[ 6],  9, 0xC040B340);
        GG(c, d, a, b, M[11], 14, 0x265E5A51); GG(b, c, d, a, M[ 0], 20, 0xE9B6C7AA);
        GG(a, b, c, d, M[ 5],  5, 0xD62F105D); GG(d, a, b, c, M[10],  9, 0x02441453);
        GG(c, d, a, b, M[15], 14, 0xD8A1E681); GG(b, c, d, a, M[ 4], 20, 0xE7D3FBC8);
        GG(a, b, c, d, M[ 9],  5, 0x21E1CDE6); GG(d, a, b, c, M[14],  9, 0xC33707D6);
        GG(c, d, a, b, M[ 3], 14, 0xF4D50D87); GG(b, c, d, a, M[ 8], 20, 0x455A14ED);
        GG(a, b, c, d, M[13],  5, 0xA9E3E905); GG(d, a, b, c, M[ 2],  9, 0xFCEFA3F8);
        GG(c, d, a, b, M[ 7], 14, 0x676F02D9); GG(b, c, d, a, M[12], 20, 0x8D2A4C8A);

        // Round 3
        HH(a, b, c, d, M[ 5],  4, 0xFFFA3942); HH(d, a, b, c, M[ 8], 11, 0x8771F681);
        HH(c, d, a, b, M[11], 16, 0x6D9D6122); HH(b, c, d, a, M[14], 23, 0xFDE5380C);
        HH(a, b, c, d, M[ 1],  4, 0xA4BEEA44); HH(d, a, b, c, M[ 4], 11, 0x4BDECFA9);
        HH(c, d, a, b, M[ 7], 16, 0xF6BB4B60); HH(b, c, d, a, M[10], 23, 0xBEBFBC70);
        HH(a, b, c, d, M[13],  4, 0x289B7EC6); HH(d, a, b, c, M[ 0], 11, 0xEAA127FA);
        HH(c, d, a, b, M[ 3], 16, 0xD4EF3085); HH(b, c, d, a, M[ 6], 23, 0x04881D05);
        HH(a, b, c, d, M[ 9],  4, 0xD9D4D039); HH(d, a, b, c, M[12], 11, 0xE6DB99E5);
        HH(c, d, a, b, M[15], 16, 0x1FA27CF8); HH(b, c, d, a, M[ 2], 23, 0xC4AC5665);

        // Round 4
        II(a, b, c, d, M[ 0],  6, 0xF4292244); II(d, a, b, c, M[ 7], 10, 0x432AFF97);
        II(c, d, a, b, M[14], 15, 0xAB9423A7); II(b, c, d, a, M[ 5], 21, 0xFC93A039);
        II(a, b, c, d, M[12],  6, 0x655B59C3); II(d, a, b, c, M[ 3], 10, 0x8F0CCC92);
        II(c, d, a, b, M[10], 15, 0xFFEFF47D); II(b, c, d, a, M[ 1], 21, 0x85845DD1);
        II(a, b, c, d, M[ 8],  6, 0x6FA87E4F); II(d, a, b, c, M[15], 10, 0xFE2CE6E0);
        II(c, d, a, b, M[ 6], 15, 0xA3014314); II(b, c, d, a, M[13], 21, 0x4E0811A1);
        II(a, b, c, d, M[ 4],  6, 0xF7537E82); II(d, a, b, c, M[11], 10, 0xBD3AF235);
        II(c, d, a, b, M[ 2], 15, 0x2AD7D2BB); II(b, c, d, a, M[ 9], 21, 0xEB86D391);

        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
    }

    static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
    static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
    static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
    static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }

    static inline uint32_t rotl(uint32_t val, int shift) {
        return (val << shift) | (val >> (32 - shift));
    }

    static inline void FF(uint32_t& a, uint32_t b, uint32_t c, uint32_t d,
                          uint32_t m, int s, uint32_t t) {
        a = b + rotl(a + F(b, c, d) + m + t, s);
    }
    static inline void GG(uint32_t& a, uint32_t b, uint32_t c, uint32_t d,
                          uint32_t m, int s, uint32_t t) {
        a = b + rotl(a + G(b, c, d) + m + t, s);
    }
    static inline void HH(uint32_t& a, uint32_t b, uint32_t c, uint32_t d,
                          uint32_t m, int s, uint32_t t) {
        a = b + rotl(a + H(b, c, d) + m + t, s);
    }
    static inline void II(uint32_t& a, uint32_t b, uint32_t c, uint32_t d,
                          uint32_t m, int s, uint32_t t) {
        a = b + rotl(a + I(b, c, d) + m + t, s);
    }

    uint32_t m_state[4];
    uint64_t m_count;
    uint8_t m_buffer[BLOCK_SIZE];
};

} // namespace md5
} // namespace douyin
