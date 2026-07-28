#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "process_launcher.h"
#include "updater.h"

namespace tuxblox {

enum class Tab { Start, About };

struct AppSnapshot {
    UpdateProgress update;
    bool playerRunning = false;
    bool studioRunning = false;
    bool playerActionInFlight = false;
    bool studioActionInFlight = false;
    std::string playerError;
    std::string studioError;
    Tab activeTab = Tab::Start;
};

class App {
public:
    App(std::string installDir, std::string currentVersion);
    ~App();

    void startUpdateCheck();

    // True once the update check has determined a verified installer
    // binary is ready to be exec'd -- the caller should stop the render
    // loop, exec() installerHandoffPath(), and never return.
    bool needsInstallerHandoff() const;
    std::string installerHandoffPath() const;

    void setActiveTab(Tab tab);
    void requestLaunch(LaunchTarget target);
    void requestStop(LaunchTarget target);
    void pollProcesses();
    AppSnapshot snapshot() const;

private:
    void updateCheckThreadMain();
    void launchThreadMain(LaunchTarget target);
    void stopThreadMain(LaunchTarget target);

    std::string installDir_;
    std::string currentVersion_;

    mutable std::mutex mutex_;
    AppSnapshot snapshot_;
    std::atomic<bool> needsInstallerHandoff_{false};
    std::string installerHandoffPath_; // written once, before needsInstallerHandoff_ is set -- see updateCheckThreadMain
    std::atomic<bool> updateCancel_{false};
    std::atomic<bool> playerActionInFlight_{false};
    std::atomic<bool> studioActionInFlight_{false};

    ProcessLauncher processLauncher_;
    std::thread updateThread_;
    std::thread playerActionThread_;
    std::thread studioActionThread_;
};

} // namespace tuxblox
