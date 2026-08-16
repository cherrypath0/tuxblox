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

#pragma once
#include <QWidget>
#include <optional>
#include <string>

class QLabel;
class QPushButton;

namespace tuxblox {

// Edge-anchored toast shown by MainWindow whenever
// AppSnapshot::updateAvailableVersion is set (Settings::autoUpdate off and
// an update is staged). Clicking the body requests the update now
// (reusing the same handoff App::requestUpdateNow() promotes); clicking
// the small dismiss control hides it for the rest of this run without
// updating. MainWindow owns positioning (edge-anchoring within the
// window) -- this widget only knows its own content and size.
class UpdatePopup : public QWidget {
    Q_OBJECT
public:
    explicit UpdatePopup(QWidget* parent = nullptr);

    void setAvailableVersion(const std::optional<std::string>& version);

signals:
    void updateRequested();
    void dismissRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    QLabel* iconLabel_ = nullptr;
    QLabel* textLabel_ = nullptr;
    QPushButton* dismissButton_ = nullptr;
};

} // namespace tuxblox
