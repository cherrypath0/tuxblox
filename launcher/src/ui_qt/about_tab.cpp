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

#include "about_tab.h"
#include "boxed_list.h"
#include "icon_utils.h"
#include "open_url.h"
#include "theme.h"
#include "version.h"
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <array>

namespace tuxblox {

namespace {

struct LinkRow { const char* label; const char* url; const char* icon; };

constexpr std::array<LinkRow, 5> kLinks = {{
    {"Website",        "https://tuxblox.net",         ":/icons/globe.png"},
    {"Documentation",  "https://tuxblox.net/docs",     ":/icons/docs.png"},
    {"GitHub",         "https://tuxblox.net/github",   ":/icons/github.png"},
    {"Discord",        "https://tuxblox.net/discord",  ":/icons/discord.png"},
    {"Privacy Policy", "https://tuxblox.net/privacy",  ":/icons/privacy.png"},
}};

} // namespace

AboutTab::AboutTab(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 16);
    layout->setSpacing(0);

    auto* title = new QLabel("About TuxBlox", this);
    title->setObjectName("pageTitle");
    title->setFont(theme::displayFont(21, QFont::Bold));
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        "A free, open source launcher and compatibility layer for running Roblox on Linux.", this);
    subtitle->setObjectName("pageSubtitle");
    subtitle->setWordWrap(true);
    layout->addSpacing(4);
    layout->addWidget(subtitle);

    layout->addSpacing(18);
    layout->addWidget(makeSectionLabel("Links", this));
    layout->addSpacing(6);

    for (const auto& link : kLinks) {
        auto* button = new QPushButton(paddedIcon(link.icon, 16, 8), link.label, this);
        button->setObjectName("linkRow");
        button->setIconSize(iconSizeWithGap(16, 8));
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        connect(button, &QPushButton::clicked, this, [url = link.url] { openUrl(url); });
        layout->addWidget(button);
    }

    layout->addStretch(1);

    auto* strip = new QWidget(this);
    strip->setObjectName("statusStrip");
    strip->setAttribute(Qt::WA_StyledBackground, true);
    auto* stripRow = new QHBoxLayout(strip);
    stripRow->setContentsMargins(0, 11, 0, 0);
    stripRow->setSpacing(6);

    auto* version = new QLabel(QString("TuxBlox %1").arg(kTuxBloxVersion), strip);
    version->setObjectName("statusText");
    stripRow->addWidget(version);
    stripRow->addStretch(1);

    auto* copyright = new QLabel(QString::fromUtf8("\xC2\xA9 2026 TuxBlox Project"), strip);
    copyright->setObjectName("statusMuted");
    stripRow->addWidget(copyright);

    layout->addWidget(strip);
}

} // namespace tuxblox
