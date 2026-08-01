#pragma once
#include "app.h"

struct ImFont;

namespace tuxblox {

class Ui {
public:
    Ui();
    ~Ui();

    bool init();
    void shutdown();
    bool renderFrame(App& app);

private:
    void* window_ = nullptr;
    void* glContext_ = nullptr;
    unsigned int logoTexture_ = 0;
    int logoWidth_ = 0, logoHeight_ = 0;
    unsigned int playerIconTexture_ = 0;
    unsigned int studioIconTexture_ = 0;
    unsigned int homeIconTexture_ = 0;
    unsigned int infoIconTexture_ = 0;
    unsigned int globeIconTexture_ = 0;
    unsigned int docsIconTexture_ = 0;
    unsigned int githubIconTexture_ = 0;
    unsigned int discordIconTexture_ = 0;
    unsigned int settingsIconTexture_ = 0;
    ImFont* fontRegular_ = nullptr;
    ImFont* fontSemiBold_ = nullptr;
    bool shouldClose_ = false;

    // Settings tab InputText backing buffers. Populated once from the
    // App's settings the first time the tab is rendered (settingsBuffersInitialized_
    // guards that) -- ImGui's InputText needs a stable buffer across
    // frames, and nothing else writes these fields while the UI is open.
    char protonEnvVarsBuf_[512] = {};
    char globalEnvVarsBuf_[512] = {};
    bool settingsBuffersInitialized_ = false;
};

} // namespace tuxblox
