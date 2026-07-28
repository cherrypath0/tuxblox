#pragma once
#include <cstddef>
#include <string>

namespace tuxblox {

std::string sha256File(const std::string& path);
std::string sha256Bytes(const unsigned char* data, size_t len);

} // namespace tuxblox
