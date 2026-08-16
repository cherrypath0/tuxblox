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

#include "theme.h"

namespace tuxblox::theme {

QString stylesheet() {
    return QString(R"(
        QMainWindow, QWidget#centralWidget { background: %1; }

        QWidget#sidebar {
            background: %2;
            border-right: 1px solid rgba(255, 255, 255, 0.14);
        }
        QPushButton#sidebarItem {
            text-align: left;
            padding: 8px 16px;
            border: none;
            border-radius: 6px;
            color: %8;
            font-size: 13px;
        }
        QPushButton#sidebarItem:checked {
            background: %4;
            color: white;
            font-weight: 600;
        }
        QPushButton#sidebarItem:!checked:hover {
            background: rgba(255, 255, 255, 0.06);
        }

        QWidget#card {
            background: %6;
            border: 1px solid %3;
            border-radius: 10px;
        }

        QPushButton#launchButton {
            background: %4;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 12px;
            font-size: 15px;
            font-weight: 600;
        }
        QPushButton#launchButton:hover { background: #4aa3ff; }
        QPushButton#launchButton:disabled { background: rgba(47, 143, 239, 0.4); }

        QPushButton#toolbarButton {
            background: %4;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#toolbarButton:hover { background: #4aa3ff; }
        QPushButton#toolbarButton:disabled { background: rgba(47, 143, 239, 0.4); }

        QPushButton#linkRow {
            text-align: left;
            padding: 8px;
            border: none;
            border-radius: 6px;
            color: %7;
        }
        QPushButton#linkRow:hover { background: rgba(255, 255, 255, 0.06); }

        QLabel#sectionTitle { color: %7; font-size: 18px; font-weight: 600; }
        QLabel#fieldLabel { color: %7; font-size: 13px; }
        QLabel#fieldHint { color: %8; font-size: 12px; }
        QLabel#footer { color: %8; font-size: 11px; }
        QLabel#errorBanner {
            background: %9;
            color: %10;
            border-radius: 6px;
            padding: 10px;
        }

        QLineEdit, QComboBox {
            background: %6;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 6px 8px;
            color: %7;
        }

        QPushButton#dangerButton {
            background: %5;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
        }
        QPushButton#dangerButton:hover { background: %11; }
        QPushButton#dangerButton:disabled { color: rgba(255, 255, 255, 0.6); }

        QWidget#dangerZone {
            border: 1px solid #7a2626;
            border-radius: 10px;
            background: rgba(122, 38, 38, 0.08);
        }

        QWidget#updatePopup {
            background: %4;
            border: 1px solid %4;
            border-radius: 10px;
        }
        QLabel#updatePopupText { color: white; font-size: 13px; font-weight: 600; }
        QPushButton#updatePopupDismiss {
            background: transparent;
            border: none;
            color: rgba(255, 255, 255, 0.85);
            font-size: 14px;
        }
        QPushButton#updatePopupDismiss:hover { color: white; }
    )")
        .arg(kBg, kSidebarBg, kBorder, kAccentBlue, kDanger, kPanelBg,
             kTextPrimary, kTextMuted, kErrorBannerBg, kErrorBannerText)
        .arg(kDangerArmed);
}

} // namespace tuxblox::theme
