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

#include "message_box.h"
#include <QApplication>
#include <QMessageBox>

namespace tuxblox {

namespace {

// QApplication needs a live argc/argv for its lifetime -- kept as static
// storage so a QApplication constructed here (only when the caller hasn't
// already made one, e.g. watch_launch.cpp's crash-popup path) stays valid
// for as long as the message box needs it. Only ever constructed once per
// process: guarded by QApplication::instance() being null.
void ensureApplication() {
    if (QApplication::instance()) return;
    static int argc = 1;
    static char appName[] = "tuxblox";
    static char* argv[] = {appName, nullptr};
    static QApplication app(argc, argv);
}

} // namespace

void showErrorMessageBox(const std::string& title, const std::string& message) {
    ensureApplication();
    QMessageBox::critical(nullptr, QString::fromStdString(title), QString::fromStdString(message));
}

void showWarningMessageBox(const std::string& title, const std::string& message) {
    ensureApplication();
    QMessageBox::warning(nullptr, QString::fromStdString(title), QString::fromStdString(message));
}

} // namespace tuxblox
