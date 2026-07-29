#include "desktop_integration.h"
#include "tuxblox_logo_png.h" // generated at build time: kTuxbloxLogoPng[], kTuxbloxLogoPngLen
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

void runCommandBestEffort(const std::vector<std::string>& argv) {
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }
    // Bounded, non-blocking wait -- xdg-mime/update-desktop-database talk to
    // a D-Bus session that can hang (this repo hit exactly this failure mode
    // once before, see 09b369a13). Give it up to ~3s, then give up rather
    // than block the caller indefinitely; we deliberately don't kill a
    // straggler process afterward -- it's harmless to leave running, and
    // this is best-effort desktop integration, not worth SIGKILL complexity.
    for (int i = 0; i < 30; ++i) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || r < 0) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace

void ensureDesktopIntegration(const std::string& installDir, const std::string& launcherExePath) {
    try {
        const std::string iconPath = installDir + "/tuxblox.png";
        {
            std::ofstream iconFile(iconPath, std::ios::binary);
            if (!iconFile) return;
            iconFile.write(reinterpret_cast<const char*>(kTuxbloxLogoPng),
                            static_cast<std::streamsize>(kTuxbloxLogoPngLen));
            if (!iconFile) return;
        }

        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') return;
        const std::string appsDir = std::string(home) + "/.local/share/applications";

        std::error_code ec;
        fs::create_directories(appsDir, ec);
        if (ec) return;

        // Main entry, with quick-launch/documentation Desktop Actions.
        {
            std::ofstream f(appsDir + "/tuxblox-launcher.desktop");
            if (!f) return;
            f <<
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=TuxBlox\n"
                "Comment=Launch TuxBlox (Roblox on Linux via Proton)\n"
                "Exec=\"" << launcherExePath << "\"\n"
                "Icon=" << iconPath << "\n"
                "Terminal=false\n"
                // Must match the SDL_VIDEO_X11_WMCLASS value set in Ui::init()
                // -- lets desktop environments match the running window back
                // to this pinned launcher, so closing it doesn't leave the
                // taskbar/dock pin showing a blank icon.
                "StartupWMClass=tuxblox-launcher\n"
                "Categories=Game;\n"
                "Actions=LaunchPlayer;LaunchStudio;Documentation;\n"
                "\n"
                "[Desktop Action LaunchPlayer]\n"
                "Name=Launch Roblox Player\n"
                "Exec=\"" << launcherExePath << "\" --launch-player\n"
                "\n"
                "[Desktop Action LaunchStudio]\n"
                "Name=Launch Roblox Studio\n"
                "Exec=\"" << launcherExePath << "\" --launch-studio\n"
                "\n"
                "[Desktop Action Documentation]\n"
                "Name=Documentation\n"
                "Exec=xdg-open https://tuxblox.net/docs\n";
        }

        // URL-scheme handler entry (not shown in app grids).
        {
            std::ofstream f(appsDir + "/tuxblox-url-handler.desktop");
            if (!f) return;
            f <<
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=TuxBlox URL Handler\n"
                "Exec=\"" << launcherExePath << "\" %u\n"
                "NoDisplay=true\n"
                "Terminal=false\n"
                "MimeType=x-scheme-handler/roblox-player;x-scheme-handler/roblox-studio;x-scheme-handler/roblox-studio-auth;\n";
        }

        if (!std::getenv("TUXBLOX_SKIP_XDG_MIME")) { // escape hatch for sandboxed test/CI runs
            runCommandBestEffort({"xdg-mime", "default", "tuxblox-url-handler.desktop", "x-scheme-handler/roblox-player"});
            runCommandBestEffort({"xdg-mime", "default", "tuxblox-url-handler.desktop", "x-scheme-handler/roblox-studio"});
            runCommandBestEffort({"xdg-mime", "default", "tuxblox-url-handler.desktop", "x-scheme-handler/roblox-studio-auth"});
            runCommandBestEffort({"update-desktop-database", appsDir});
        }
    } catch (...) {
        // Best-effort -- must never fail an otherwise-working launch.
    }
}

} // namespace tuxblox
