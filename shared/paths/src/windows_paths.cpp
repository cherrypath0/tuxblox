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

#include "windows_paths.h"
#include <array>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace tuxblox {
namespace {

struct NameCase {
    const char* Lower;
    const char* Canonical;
};

// The same folders the compatibility layer capitalises in the paths it hands back.
// Wine keeps its own C copy of this table, which cannot share a source file with
// this one because the two halves carry different licences, so the two must be
// changed together. Longer names come first so that "program files (x86)" is not
// taken for "program files".
const std::array<NameCase, 5> RootNames = {{
    {"program files (x86)", "Program Files (x86)"},
    {"program files",       "Program Files"},
    {"programdata",         "ProgramData"},
    {"windows",             "Windows"},
    {"users",               "Users"}
}};

const std::array<NameCase, 2> WindowsNames = {{
    {"system32", "System32"},
    {"syswow64", "SysWOW64"}
}};

char lowerAscii(char character) {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character + ('a' - 'A'));
    }
    return character;
}

// The name has to run to the end of the component, so "usersdata" is not "users"
bool componentMatches(const std::string& value, size_t at, const char* name) {
    size_t index = 0;
    for (; name[index] != '\0'; index++) {
        if (at + index >= value.size() || lowerAscii(value[at + index]) != name[index]) {
            return false;
        }
    }
    const size_t end = at + index;
    return end >= value.size() || value[end] == '"' || value[end] == '\\';
}

// Returns where the component ends, or npos when none of the names matched
size_t rewriteComponent(std::string& value, size_t at,
        const NameCase* names, size_t count) {
    for (size_t index = 0; index < count; index++) {
        if (!componentMatches(value, at, names[index].Lower)) {
            continue;
        }
        const std::string canonical = names[index].Canonical;
        value.replace(at, canonical.size(), canonical);
        return at + canonical.size();
    }
    return std::string::npos;
}

bool isWordCharacter(char character) {
    const char lower = lowerAscii(character);
    return (lower >= 'a' && lower <= 'z') || (character >= '0' && character <= '9');
}

// Registry values double their backslashes and plain paths do not, so the one
// separator length is all that differs between the two forms.
std::string canonicalise(const std::string& value, size_t separator) {
    const std::string slashes(separator, '\\');
    const std::string root = ":" + slashes;
    std::string result = value;
    size_t at = 0;

    // A value holds its paths anywhere: after @=, inside str(2):, or quoted
    // again inside a command line, so every drive root in the text is taken.
    while ((at = result.find(root, at)) != std::string::npos) {
        const size_t letter = at;
        at += root.size();
        if (letter == 0) {
            continue;
        }

        const char drive = lowerAscii(result[letter - 1]);
        if (drive < 'a' || drive > 'z') {
            continue;
        }
        // The drive letter stands on its own, so "dword:" and "abc:" are not drives
        if (letter >= 2 && isWordCharacter(result[letter - 2])) {
            continue;
        }

        const size_t after = rewriteComponent(result, at, RootNames.data(), RootNames.size());
        if (after == std::string::npos) {
            continue;
        }

        // system32 and syswow64 are canonical under windows and nowhere else
        if (result.compare(at, after - at, "Windows") == 0 &&
                result.compare(after, separator, slashes) == 0) {
            rewriteComponent(result, after + separator, WindowsNames.data(), WindowsNames.size());
        }
        at = after;
    }
    return result;
}

} // namespace

std::string canonicalWindowsPath(const std::string& value) {
    return canonicalise(value, 2);
}

std::string canonicalWindowsPathLiteral(const std::string& path) {
    return canonicalise(path, 1);
}

bool canonicaliseRegFile(const fs::path& file) {
    std::ifstream in(file);
    if (!in) {
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    const std::string corrected = canonicalWindowsPath(text);
    if (corrected == text) {
        return false;
    }

    // Written beside the file and renamed over it, so a launch interrupted
    // here leaves the drive with its old registry rather than half of one.
    const fs::path temporary = file.string() + ".tuxblox-new";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << corrected;
        if (!out) {
            return false;
        }
    }

    std::error_code error;
    fs::rename(temporary, file, error);
    if (error) {
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

} // namespace tuxblox
