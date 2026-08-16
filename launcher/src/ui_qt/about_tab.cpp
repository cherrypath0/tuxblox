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
#include "icon_utils.h"
#include "open_url.h"
#include "version.h"
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
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    auto* logo = new QLabel(this);
    logo->setPixmap(QIcon(":/branding/tuxblox_logo.png").pixmap(72, 72));
    logo->setAlignment(Qt::AlignHCenter);
    layout->addWidget(logo);

    auto* title = new QLabel("About TuxBlox", this);
    title->setObjectName("sectionTitle");
    title->setAlignment(Qt::AlignHCenter);
    layout->addWidget(title);

    layout->addSpacing(8);

    for (const auto& link : kLinks) {
        auto* button = new QPushButton(paddedIcon(link.icon, 22, 6), link.label, this);
        button->setObjectName("linkRow");
        button->setIconSize(iconSizeWithGap(22, 6));
        button->setFlat(true);
        connect(button, &QPushButton::clicked, this, [url = link.url] { openUrl(url); });
        layout->addWidget(button);
    }

    layout->addStretch(1);

    auto* versionFooter = new QLabel(QString("TuxBlox v%1").arg(kTuxBloxVersion), this);
    versionFooter->setObjectName("footer");
    layout->addWidget(versionFooter);

    auto* copyrightFooter = new QLabel(QString::fromUtf8("\xC2\xA9 2026 TuxBlox Project"), this);
    copyrightFooter->setObjectName("footer");
    layout->addWidget(copyrightFooter);
}

} // namespace tuxblox
