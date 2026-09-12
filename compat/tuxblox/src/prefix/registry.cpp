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
//
// Portions derived from Proton's proton.py:
// Copyright (c) 2018-2022, Valve Corporation. All rights reserved.
// Licensed under the 3-clause BSD license; see
// third_party_licenses/proton/LICENSE.proton for the full text.

#include "prefix/registry.h"
#include "support/util.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::vector<std::string> readLines(const fs::path& file, bool& ok) {
    std::vector<std::string> lines;
    std::ifstream in(file);
    if (!in) {
        ok = false;
        return lines;
    }
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line + "\n");
    }
    ok = true;
    return lines;
}

// Writes the file next to the original and renames it over the top, so a
// failure part-way through leaves the original registry intact.
bool writeAtomically(const fs::path& file, const std::vector<std::string>& lines) {
    const fs::path staged = file.string() + ".new";
    {
        std::ofstream out(staged);
        if (!out) {
            log("Unable to write new registry file to " + file.string());
            return false;
        }
        for (const std::string& line : lines) {
            out << line;
        }
        if (!out) {
            log("Unable to write new registry file to " + file.string());
            return false;
        }
    }

    std::error_code error;
    fs::permissions(staged, fs::status(file, error).permissions(),
                    fs::perm_options::replace, error);
    fs::rename(staged, file, error);
    if (error) {
        log("Unable to write new registry file to " + file.string());
        fs::remove(staged, error);
        return false;
    }
    return true;
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool isBlank(const std::string& line) {
    return line.find_first_not_of(" \t\r\n") == std::string::npos;
}

// Walks a .reg file looking for `name` inside `key`. Returns the old value and,
// when newValue is given, rewrites the file with it in place.
std::string findOrReplace(const fs::path& file, const std::string& key,
                          const std::string& name, const std::string *pNewValue) {
    bool ok = false;
    std::vector<std::string> lines = readLines(file, ok);
    if (!ok) {
        return "";
    }

    const std::string nameStr = "\"" + name + "\"=";
    const std::string header = "[" + key + "]";
    bool foundKey = false;
    bool replaced = false;
    std::string oldValue;

    for (size_t idx = 0; idx < lines.size(); idx++) {
        const std::string& line = lines[idx];
        if (replaced) {
            break;
        }
        if (!line.empty() && line[0] == '[') {
            // Past the key's own block without a match.
            if (foundKey) {
                return "";
            }
            // A subkey header is "[key\\sub]", so it cannot match "[key]".
            if (startsWith(line, header)) {
                foundKey = true;
            }
            continue;
        }
        if (!foundKey) {
            continue;
        }
        const size_t at = line.find(nameStr);
        if (at == std::string::npos) {
            continue;
        }
        oldValue = line.substr(at + nameStr.size());
        while (!oldValue.empty() && (oldValue.back() == '\n' || oldValue.back() == '\r')) {
            oldValue.pop_back();
        }
        if (pNewValue == nullptr) {
            return oldValue;
        }
        lines[idx] = nameStr + *pNewValue + "\n";
        replaced = true;
    }

    if (replaced) {
        writeAtomically(file, lines);
    }
    return oldValue;
}

} // namespace

std::string getRegValue(const fs::path& file, const std::string& key, const std::string& name) {
    return findOrReplace(file, key, name, nullptr);
}

std::string replaceRegValue(const fs::path& file, const std::string& key,
                            const std::string& name, const std::string& newValue) {
    return findOrReplace(file, key, name, &newValue);
}

bool setRegKeyValues(const fs::path& file, const std::string& key,
                     const std::map<std::string, std::string>& values) {
    bool ok = false;
    std::vector<std::string> lines = readLines(file, ok);
    if (!ok) {
        return false;
    }

    const std::string header = "[" + key + "]";
    size_t start = lines.size();
    for (size_t idx = 0; idx < lines.size(); idx++) {
        if (startsWith(lines[idx], header)) {
            start = idx;
            break;
        }
    }

    if (start == lines.size()) {
        // New key. Wine wants "[key] <unix seconds>" plus the 100ns-since-1601
        // stamp it writes itself; it rewrites both on its next flush.
        const long long now = static_cast<long long>(std::time(nullptr));
        char stamp[64];
        std::snprintf(stamp, sizeof(stamp), "#time=%llx\n",
                      (now + 11644473600LL) * 10000000LL);

        lines.push_back("\n");
        lines.push_back(header + " " + std::to_string(now) + "\n");
        lines.push_back(stamp);
        for (const auto& [name, value] : values) {
            lines.push_back("\"" + name + "\"=" + value + "\n");
        }
    } else {
        // The key's block runs to the next key header or the end of the file.
        size_t end = lines.size();
        for (size_t idx = start + 1; idx < lines.size(); idx++) {
            if (!lines[idx].empty() && lines[idx][0] == '[') {
                end = idx;
                break;
            }
        }

        std::vector<std::string> block(lines.begin() + static_cast<long>(start),
                                       lines.begin() + static_cast<long>(end));

        // Appends go before the blank line that separates blocks, if any.
        size_t insertAt = block.size();
        while (insertAt > 0 && isBlank(block[insertAt - 1])) {
            insertAt--;
        }

        bool changed = false;
        for (const auto& [name, value] : values) {
            const std::string nameStr = "\"" + name + "\"=";
            const std::string line = nameStr + value + "\n";
            bool found = false;
            for (std::string& existing : block) {
                if (startsWith(existing, nameStr)) {
                    if (existing != line) {
                        existing = line;
                        changed = true;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                block.insert(block.begin() + static_cast<long>(insertAt), line);
                insertAt++;
                changed = true;
            }
        }

        if (!changed) {
            return false;
        }

        std::vector<std::string> rebuilt(lines.begin(), lines.begin() + static_cast<long>(start));
        rebuilt.insert(rebuilt.end(), block.begin(), block.end());
        rebuilt.insert(rebuilt.end(), lines.begin() + static_cast<long>(end), lines.end());
        lines = std::move(rebuilt);
    }

    return writeAtomically(file, lines);
}

} // namespace tuxblox
