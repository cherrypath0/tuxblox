#pragma once
#include <string>
#include <vector>

namespace tuxblox {

enum class LaunchTarget { Player, Studio };

const char* targetExeName(LaunchTarget target);
const char* targetLnkRelPath(LaunchTarget target);

std::string extractExeRelPathFromLnkBytes(const std::vector<unsigned char>& lnkBytes,
                                           const std::string& targetExeName);

std::string resolveExePath(LaunchTarget target, const std::string& driveCRoot);

} // namespace tuxblox
