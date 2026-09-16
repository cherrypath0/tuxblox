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

// Checks that a Windows program is the one its publisher signed.
//
// This has to be done here rather than through Windows' own WinVerifyTrust,
// which the compatibility layer answers: that path needs a decoder for
// SPC_INDIRECT_DATA that Wine declares but never implemented, so it fails on
// every signed file, genuine or not.

#ifndef TUXBLOX_INTEGRITY_AUTHENTICODE_H
#define TUXBLOX_INTEGRITY_AUTHENTICODE_H

#include <filesystem>
#include <string>
#include <string_view>

namespace tuxblox {

enum class IntegrityStatus {
    // The file matches its signature, and the signature chains to a root we
    // trust. This is the only value that means "safe to run".
    Verified,
    // The file carries no signature at all.
    NotSigned,
    // The file is signed, but its contents no longer match what was signed.
    Modified,
    // The signature itself does not hold up: a broken signature, or a chain
    // that does not reach a root we trust.
    BadSignature,
    // Signed and intact, but by somebody else.
    WrongPublisher,
    // Not readable, or not a Windows program at all.
    Unreadable
};

struct IntegrityReport {
    IntegrityStatus status = IntegrityStatus::Unreadable;
    // One line for the log, always set.
    std::string detail;
    // The organisation that signed it, when the file got far enough for that
    // to be known.
    std::string publisher;
};

// Verifies file and requires it to have been signed by requiredPublisher
// (matched against the signing certificate's organisation name). Roots is the
// PEM text of every root certificate to trust.
IntegrityReport verifyAuthenticode(const std::filesystem::path& file,
                                   const std::string& requiredPublisher,
                                   std::string_view rootsPem);

// Human-readable name for a status, for logs.
const char *integrityStatusName(IntegrityStatus status);

} // namespace tuxblox

#endif // TUXBLOX_INTEGRITY_AUTHENTICODE_H
