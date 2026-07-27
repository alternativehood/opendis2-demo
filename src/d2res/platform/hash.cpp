#include "hash.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
// Minimal SHA-256 implementation (FIPS 180-4).
// This is original code written from the published standard; no license restrictions.
#include <cstring>
namespace {

constexpr std::array<uint32_t, 64> kK = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

struct Sha256Ctx {
    uint64_t                bit_count = 0;
    uint32_t                buf_len = 0;
    std::array<uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t                 buf[64] = {};

    void process_block(const uint8_t* blk) {
        uint32_t w[64];
        for (std::size_t i = 0; i < 16; ++i)
            w[i] = (uint32_t(blk[i * 4]) << 24) | (uint32_t(blk[i * 4 + 1]) << 16) |
                   (uint32_t(blk[i * 4 + 2]) << 8) | uint32_t(blk[i * 4 + 3]);
        for (std::size_t i = 16; i < 64; ++i) {
            uint32_t const s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t const s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        auto [a, b, c, d, e, f, g, h] = state;
        for (std::size_t i = 0; i < 64; ++i) {
            uint32_t const S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
            uint32_t const ch = (e & f) ^ (~e & g);
            uint32_t const tmp1 = h + S1 + ch + kK[i] + w[i];
            uint32_t const S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
            uint32_t const maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t const tmp2 = S0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + tmp1;
            d = c;
            c = b;
            b = a;
            a = tmp1 + tmp2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    void update(const uint8_t* data, std::size_t len) {
        bit_count += len * 8;
        while (len > 0) {
            uint32_t const space = 64 - buf_len;
            uint32_t const take = (len < space) ? static_cast<uint32_t>(len) : space;
            std::memcpy(buf + buf_len, data, take);
            buf_len += take;
            data += take;
            len -= take;
            if (buf_len == 64) {
                process_block(buf);
                buf_len = 0;
            }
        }
    }

    std::array<uint8_t, 32> finalize() {
        buf[buf_len++] = 0x80;
        if (buf_len > 56) {
            while (buf_len < 64)
                buf[buf_len++] = 0;
            process_block(buf);
            buf_len = 0;
        }
        while (buf_len < 56)
            buf[buf_len++] = 0;
        for (std::size_t i = 8; i-- > 0;)
            buf[buf_len++] = static_cast<uint8_t>(bit_count >> (i * 8));
        process_block(buf);
        std::array<uint8_t, 32> digest{};
        for (std::size_t i = 0; i < 8; ++i)
            for (std::size_t j = 0; j < 4; ++j)
                digest[i * 4 + j] = static_cast<uint8_t>(state[i] >> (24 - j * 8));
        return digest;
    }
};

} // namespace
#endif

namespace d2res::platform {

std::string sha256_hex(std::span<const uint8_t> data) {
#ifdef __APPLE__
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.data(), static_cast<CC_LONG>(data.size()), digest);
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (unsigned char const b : digest)
        hex << std::setw(2) << static_cast<unsigned>(b);
    return hex.str();
#else
    Sha256Ctx ctx;
    ctx.update(data.data(), data.size());
    auto const         digest = ctx.finalize();
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (uint8_t const b : digest)
        hex << std::setw(2) << static_cast<unsigned>(b);
    return hex.str();
#endif
}

std::string sha256_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return "";
#ifdef __APPLE__
    CC_SHA256_CTX ctx;
    CC_SHA256_Init(&ctx);
    char buf[65536];
    while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
        CC_SHA256_Update(&ctx, reinterpret_cast<const uint8_t*>(buf),
                         static_cast<CC_LONG>(f.gcount()));
    }
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Final(digest, &ctx);
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (unsigned char const b : digest)
        hex << std::setw(2) << static_cast<unsigned>(b);
    return hex.str();
#else
    Sha256Ctx ctx;
    char      buf[65536];
    while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
        ctx.update(reinterpret_cast<const uint8_t*>(buf), static_cast<std::size_t>(f.gcount()));
    }
    auto const         digest = ctx.finalize();
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (uint8_t const b : digest)
        hex << std::setw(2) << static_cast<unsigned>(b);
    return hex.str();
#endif
}

} // namespace d2res::platform
