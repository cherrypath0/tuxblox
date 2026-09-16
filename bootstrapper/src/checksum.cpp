// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

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

std::string digestBytes(const EVP_MD* md, const char* fnName, const unsigned char* data, size_t len) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error(std::string(fnName) + ": EVP_MD_CTX_new failed");
    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data, len) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digestLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error(std::string(fnName) + ": digest computation failed");
    }
    EVP_MD_CTX_free(ctx);
    return toHex(digest, digestLen);
}

std::string digestFile(const EVP_MD* md, const char* fnName, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error(std::string(fnName) + ": cannot open " + path);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
        if (ctx) EVP_MD_CTX_free(ctx);
        throw std::runtime_error(std::string(fnName) + ": EVP init failed");
    }

    std::vector<char> buf(1 << 16);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = in.gcount();
        if (got > 0 && EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(got)) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error(std::string(fnName) + ": digest update failed");
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    if (EVP_DigestFinal_ex(ctx, digest, &digestLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error(std::string(fnName) + ": digest finalize failed");
    }
    EVP_MD_CTX_free(ctx);
    return toHex(digest, digestLen);
}

} // namespace

std::string sha256Bytes(const unsigned char* data, size_t len) {
    return digestBytes(EVP_sha256(), "sha256Bytes", data, len);
}

std::string sha256File(const std::string& path) {
    return digestFile(EVP_sha256(), "sha256File", path);
}

std::string md5Bytes(const unsigned char* data, size_t len) {
    return digestBytes(EVP_md5(), "md5Bytes", data, len);
}

std::string md5File(const std::string& path) {
    return digestFile(EVP_md5(), "md5File", path);
}

} // namespace tuxblox
