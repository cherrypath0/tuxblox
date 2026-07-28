#pragma once
#include <string>

namespace tuxblox {

// Writes installDir + "/COPYRIGHT.txt", the attribution file for third-party
// components bundled into the installer (currently: the Inter font, OFL-1.1).
// Format matches plan/example-COPYRIGHT.txt. Best-effort: never throws --
// failure here must not fail an otherwise-successful install.
void writeCopyrightFile(const std::string& installDir);

} // namespace tuxblox
