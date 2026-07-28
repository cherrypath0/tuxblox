#pragma once
#include <string>

namespace tuxblox {

// Writes a desktop icon file and an XDG .desktop entry ("TuxBlox Launcher")
// pointing at installDir + "/TuxBloxLauncher", so the launcher is findable
// via the desktop environment's app search. Best-effort: never throws --
// failure here must not fail an otherwise-successful install.
void createDesktopShortcut(const std::string& installDir);

} // namespace tuxblox
