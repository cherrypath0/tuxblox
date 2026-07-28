#include "checksum.h"
#include <openssl/evp.h>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace tuxblox {

namespace {

std::string toHex(const unsigned char* digest, unsigned int len) {
    static const char* hexChars = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        out.push_back(hexChars[(digest[i] >> 4) & 0xF]);
        out.push_back(hexChars[digest[i] & 0xF]);
    }
    return out;
}

} // namespace

std::string sha256Bytes(const unsigned char* data, size_t len) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("sha256Bytes: EVP_MD_CTX_new failed");
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data, len) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digestLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("sha256Bytes: digest computation failed");
    }
    EVP_MD_CTX_free(ctx);
    return toHex(digest, digestLen);
}

std::string sha256File(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("sha256File: cannot open " + path);
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        if (ctx) EVP_MD_CTX_free(ctx);
        throw std::runtime_error("sha256File: EVP init failed");
    }

    std::vector<char> buf(1 << 16);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = in.gcount();
        if (got > 0) {
            if (EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(got)) != 1) {
                EVP_MD_CTX_free(ctx);
                throw std::runtime_error("sha256File: digest update failed");
            }
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    if (EVP_DigestFinal_ex(ctx, digest, &digestLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("sha256File: digest finalize failed");
    }
    EVP_MD_CTX_free(ctx);
    return toHex(digest, digestLen);
}

} // namespace tuxblox
