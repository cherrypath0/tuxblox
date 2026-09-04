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

namespace {

QString SansFamily = "Inter";
QString DisplayFamily = "Montserrat";

// The stylesheet below is written with %name% placeholders rather than
// QString::arg()'s %1..%9, which runs out of slots long before this many
// tokens and makes every edit a renumbering exercise.
struct Token { const char* name; const char* value; };

QString expandTokens(QString css) {
    const Token tokens[] = {
        {"%bg%", kBg},
        {"%bgAlt%", kBgAlt},
        {"%bgSoft%", kBgSoft},
        {"%bgElevated%", kBgElevated},
        {"%divider%", kDivider},
        {"%dividerSoft%", kDividerSoft},
        {"%brand%", kBrand},
        {"%brandSoft%", kBrandSoft},
        {"%accent%", kAccent},
        {"%text1%", kTextPrimary},
        {"%text2%", kTextMuted},
        {"%text3%", kTextMutedDim},
        {"%btnBrand%", kButtonBrandBg},
        {"%btnBrandHover%", kButtonBrandHover},
        {"%btnAlt%", kButtonAltBg},
        {"%btnAltHover%", kButtonAltHover},
        {"%dangerBorder%", kDangerBorder},
        {"%dangerText%", kDangerText},
        {"%dangerArmed%", kDangerArmedBg},
        {"%errorBg%", kErrorBannerBg},
        {"%success%", kSuccess},
    };
    for (const auto& token : tokens) {
        css.replace(token.name, token.value);
    }
    css.replace("%radiusBtn%", QString::number(kRadiusButton) + "px");
    css.replace("%radiusCard%", QString::number(kRadiusCard) + "px");
    return css;
}

} // namespace

void setFontFamilies(const QString& sans, const QString& display) {
    SansFamily = sans;
    DisplayFamily = display;
}

QString sansFamily() { return SansFamily; }

QFont displayFont(int pixelSize, int weight) {
    QFont font(DisplayFamily);
    font.setPixelSize(pixelSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setLetterSpacing(QFont::PercentageSpacing, 98.0);
    return font;
}

QFont sectionLabelFont() {
    QFont font(SansFamily);
    font.setPixelSize(11);
    font.setWeight(QFont::DemiBold);
    font.setCapitalization(QFont::AllUppercase);
    font.setLetterSpacing(QFont::PercentageSpacing, 108.0);
    return font;
}

QString stylesheet() {
    return expandTokens(QString(R"(
        QMainWindow, QWidget#centralWidget { background: %bg%; }
        QToolTip {
            background: %bgElevated%;
            color: %text1%;
            border: 1px solid %divider%;
            border-radius: %radiusBtn%;
            padding: 4px 8px;
        }

        /* ---- sidebar: the site's docs navigation ---- */
        QWidget#sidebar {
            background: %bgAlt%;
            border-right: 1px solid %dividerSoft%;
        }
        QLabel#sidebarBrand { color: %text1%; }
        QWidget#sidebarFooter { border-top: 1px solid %dividerSoft%; }
        QPushButton#sidebarItem {
            text-align: left;
            padding: 7px 10px;
            border: none;
            border-radius: %radiusCard%;
            color: %text2%;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton#sidebarItem:!checked:hover {
            background: rgba(255, 255, 255, 0.07);
            color: %text1%;
        }
        QPushButton#sidebarItem:checked {
            background: %brandSoft%;
            color: %brand%;
            font-weight: 600;
        }

        /* ---- page furniture ---- */
        QLabel#pageTitle { color: %text1%; }
        QLabel#pageSubtitle { color: %text2%; font-size: 13px; }
        QLabel#sectionLabel { color: %text3%; }
        QWidget#statusStrip { border-top: 1px solid %dividerSoft%; }
        QLabel#statusText { color: %text1%; font-size: 12px; font-weight: 500; }
        QLabel#statusMuted { color: %text3%; font-size: 12px; }
        QLabel#statusOk { color: %text2%; font-size: 12px; }
        QLabel#statusDot { border-radius: 3px; background: %success%; }
        QLabel#statusDot[state="pending"] { background: %accent%; }
        QLabel#statusDot[state="error"] { background: %dangerText%; }

        /* ---- app cards on Home ---- */
        QFrame#appCard {
            background: %bgSoft%;
            border: 1px solid %divider%;
            border-radius: %radiusCard%;
        }
        QFrame#appCard[installed="true"] { border-color: %brand%; }
        QLabel#cardTitle { color: %text1%; }
        QLabel#cardMeta { color: %text3%; font-size: 11px; }
        QLabel#cardMeta[missing="true"] { color: %accent%; }

        /* ---- boxed list rows: grouped settings ---- */
        QFrame#boxedGroup {
            background: %bgSoft%;
            border: 1px solid %divider%;
            border-radius: %radiusCard%;
        }
        QFrame#rowSeparator { background: %dividerSoft%; border: none; }
        QLabel#rowTitle { color: %text1%; font-size: 13px; font-weight: 500; }
        QLabel#rowDesc { color: %text3%; font-size: 11px; }
        QLabel#pill {
            color: %brand%;
            background: %brandSoft%;
            border-radius: 9px;
            padding: 2px 8px;
            font-size: 10px;
            font-weight: 600;
        }

        /* ---- buttons ---- */
        QPushButton#primaryButton {
            background: %btnBrand%;
            color: white;
            border: 1px solid transparent;
            border-radius: %radiusBtn%;
            padding: 0 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover { background: %btnBrandHover%; }
        QPushButton#primaryButton:disabled { background: %bgElevated%; color: %text3%; }

        QPushButton#secondaryButton {
            background: %btnAlt%;
            color: %text1%;
            border: 1px solid %divider%;
            border-radius: %radiusBtn%;
            padding: 0 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#secondaryButton:hover { background: %btnAltHover%; }
        QPushButton#secondaryButton:disabled { color: %text3%; }

        QPushButton#toolbarButton {
            background: %btnAlt%;
            color: %text1%;
            border: 1px solid %divider%;
            border-radius: %radiusBtn%;
            padding: 5px 12px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton#toolbarButton:hover { background: %btnAltHover%; }
        QPushButton#toolbarButton:disabled { color: %text3%; }

        QPushButton#toolbarButtonBrand {
            background: %btnBrand%;
            color: white;
            border: 1px solid transparent;
            border-radius: %radiusBtn%;
            padding: 5px 12px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton#toolbarButtonBrand:hover { background: %btnBrandHover%; }
        QPushButton#toolbarButtonBrand:disabled { background: %bgElevated%; color: %text3%; }

        QPushButton#linkRow {
            text-align: left;
            padding: 8px 10px;
            border: none;
            border-radius: %radiusCard%;
            color: %text2%;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton#linkRow:hover { background: rgba(255, 255, 255, 0.07); color: %text1%; }

        QPushButton#dangerButton {
            background: transparent;
            color: %dangerText%;
            border: 1px solid %dangerBorder%;
            border-radius: %radiusBtn%;
            padding: 5px 12px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton#dangerButton:hover { background: rgba(190, 60, 60, 0.14); }
        QPushButton#dangerButton:disabled { color: %text3%; border-color: %divider%; }
        QPushButton#dangerButton[armed="true"] {
            background: %dangerArmed%;
            border-color: %dangerArmed%;
            color: white;
        }

        /* ---- inputs ---- */
        QLineEdit, QComboBox {
            background: %bg%;
            border: 1px solid %divider%;
            border-radius: %radiusBtn%;
            padding: 5px 9px;
            color: %text1%;
            font-size: 12px;
            selection-background-color: %btnBrand%;
        }
        QLineEdit:focus, QComboBox:focus { border-color: %brand%; }
        QLineEdit[duplicate="true"] { border-color: %dangerBorder%; color: %dangerText%; }
        QLineEdit::placeholder { color: %text3%; }
        QComboBox { background: %bgElevated%; }
        QComboBox::drop-down { border: none; width: 22px; }
        /* Qt draws its own style's arrow unless given a real image -- the
           CSS border-triangle trick renders as a filled box here. */
        QComboBox::down-arrow {
            image: url(:/icons/chevron-down.png);
            width: 12px;
            height: 12px;
        }
        QComboBox QAbstractItemView {
            background: %bgElevated%;
            border: 1px solid %divider%;
            border-radius: %radiusBtn%;
            color: %text1%;
            padding: 3px;
            outline: none;
            selection-background-color: %brandSoft%;
            selection-color: %brand%;
        }

        /* ---- progress and errors ---- */
        QProgressBar {
            background: %bgElevated%;
            border: none;
            border-radius: 3px;
            height: 6px;
            text-align: center;
        }
        QProgressBar::chunk { background: %brand%; border-radius: 3px; }
        QLabel#errorBanner {
            background: %errorBg%;
            color: %dangerText%;
            border: 1px solid %dangerBorder%;
            border-radius: %radiusCard%;
            padding: 9px 11px;
            font-size: 12px;
        }

        /* ---- danger zone ---- */
        QFrame#dangerZone {
            border: 1px solid %dangerBorder%;
            border-radius: %radiusCard%;
            background: rgba(190, 60, 60, 0.06);
        }

        /* ---- update toast ---- */
        QWidget#updatePopup {
            background: %bgElevated%;
            border: 1px solid %brand%;
            border-radius: %radiusCard%;
        }
        QLabel#updatePopupText { color: %text1%; font-size: 12px; font-weight: 500; }
        QPushButton#updatePopupDismiss {
            background: transparent;
            border: none;
            color: %text3%;
            font-size: 14px;
        }
        QPushButton#updatePopupDismiss:hover { color: %text1%; }

        /* ---- scrolling ---- */
        QScrollArea { background: transparent; border: none; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %divider%;
            border-radius: 5px;
            min-height: 28px;
        }
        QScrollBar::handle:vertical:hover { background: %text3%; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )"));
}

} // namespace tuxblox::theme
