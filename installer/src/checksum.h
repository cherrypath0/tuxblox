#pragma once
#include <cstddef>
#include <string>

namespace tuxblox {

// Returns the lowercase hex-encoded SHA-256 digest of the file at path.
// Throws std::runtime_error if the file cannot be opened or hashing fails.
std::string sha256File(const std::string& path);

// Returns the lowercase hex-encoded SHA-256 digest of an in-memory buffer.
std::string sha256Bytes(const unsigned char* data, size_t len);

} // namespace tuxblox
