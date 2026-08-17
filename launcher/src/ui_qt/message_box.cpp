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
//
// The QApplication itself is heap-allocated and DELIBERATELY NEVER DELETED,
// which is load-bearing rather than sloppy. As a function-local `static
// QApplication app(...)` it was destroyed during exit()'s static-destructor
// phase, and that reliably SIGSEGV'd:
//
//     #0 libQt6Gui.so.6
//     #1 qt_call_post_routines()
//     #2 QApplication::~QApplication()
//     #3/#4 __run_exit_handlers / exit()
//
// ~QApplication runs qt_call_post_routines(), which invokes routines that
// touch Qt's platform/GUI state -- but by the time atexit handlers are
// running, parts of that state have already been torn down by Qt's own exit
// hooks, so the post routines dereference freed memory. Qt's own guidance is
// that the application object's lifetime must be controlled explicitly (a
// scope in main(), as src/main.cpp:219 correctly does), never left to static
// destruction order.
//
// This path cannot use a main() scope: it exists precisely for the processes
// that never built one. Leaking is the standard remedy -- the process is on
// its way out, the OS reclaims everything, and skipping ~QApplication skips
// the crash entirely.
//
// This was not a cosmetic "crash on exit": watch_launch.cpp shows this exact
// dialog whenever a launched Roblox process exits non-zero, so every one of
// those error popups took the launcher down with it, turning one Studio
// failure into two.
void ensureApplication() {
    if (QApplication::instance()) return;
    static int argc = 1;
    static char appName[] = "tuxblox";
    static char* argv[] = {appName, nullptr};
    static QApplication* app = new QApplication(argc, argv);
    (void)app;
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
