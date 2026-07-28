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
    ImFont* fontRegular_ = nullptr;
    ImFont* fontSemiBold_ = nullptr;
    bool shouldClose_ = false;
};

} // namespace tuxblox
