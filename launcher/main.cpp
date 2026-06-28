#include <ncurses.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>

#ifndef version
#define version "unknown"
#endif

// ---------- Simple settings storage ----------
std::map<std::string, std::string> settings = {
    {"fullscreen", "false"},
    {"volume", "80"},
    {"music", "true"}
};

const std::string SETTINGS_FILE = "settings.json";

void saveSettings() {
    std::ofstream out(SETTINGS_FILE);
    out << "{\n";
    size_t i = 0;
    for (auto &pair : settings) {
        out << "  \"" << pair.first << "\": ";
        // write booleans/numbers without quotes, everything else with quotes
        if (pair.second == "true" || pair.second == "false" ||
            pair.second.find_first_not_of("0123456789") == std::string::npos) {
            out << pair.second;
        } else {
            out << "\"" << pair.second << "\"";
        }
        if (++i != settings.size()) out << ",";
        out << "\n";
    }
    out << "}\n";
    out.close();
}

void loadSettings() {
    std::ifstream in(SETTINGS_FILE);
    if (!in.is_open()) return; // no file yet, keep defaults

    std::string line;
    while (std::getline(in, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // strip quotes, braces, commas, whitespace
        auto strip = [](std::string &s) {
            std::string result;
            for (char ch : s) {
                if (ch != '"' && ch != ',' && ch != '{' && ch != '}' &&
                    ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
                    result += ch;
            }
            return result;
        };

        key = strip(key);
        value = strip(value);

        if (!key.empty()) settings[key] = value;
    }
}

// ---------- Generic menu renderer ----------
// Returns the index chosen, with `selectedMark` placed before the chosen item
int runMenu(const std::string &title, const std::vector<std::string> &options) {
    int highlight = 0;
    int choice = -1;
    int c;

    while (true) {
        clear();
        mvprintw(1, 2, "%s", title.c_str());
        mvprintw(2, 2, "----------------------------");

        for (size_t i = 0; i < options.size(); i++) {
            std::string label = options[i];
            if ((int)i == highlight) {
                attron(A_REVERSE);
                mvprintw(4 + i, 4, "> %s", label.c_str());
                attroff(A_REVERSE);
            } else {
                mvprintw(4 + i, 4, "  %s", label.c_str());
            }
        }

        c = getch();
        switch (c) {
            case KEY_UP:
                highlight = (highlight - 1 + options.size()) % options.size();
                break;
            case KEY_DOWN:
                highlight = (highlight + 1) % options.size();
                break;
            case 10: // Enter key
                choice = highlight;
                break;
        }

        if (choice != -1) break;
    }

    return choice;
}

void settingsMenu() {
    while (true) {
        std::vector<std::string> options = {
            "Fullscreen: " + settings["fullscreen"],
            "Volume: " + settings["volume"],
            "Music: " + settings["music"],
            "Save & Back"
        };

        int choice = runMenu("Settings", options);

        if (choice == 0) {
            settings["fullscreen"] = (settings["fullscreen"] == "true") ? "false" : "true";
        } else if (choice == 1) {
            int vol = std::stoi(settings["volume"]);
            vol = (vol + 10) % 110; // cycles 0,10,...100,0
            settings["volume"] = std::to_string(vol);
        } else if (choice == 2) {
            settings["music"] = (settings["music"] == "true") ? "false" : "true";
        } else if (choice == 3) {
            saveSettings();
            break;
        }
    }
}

int main() {
    loadSettings();

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    std::vector<std::string> options = {"Launch Game", "Settings", "Check for Updates", "Quit"};
    int choice = -1;

    while (true) {
        std::string title = "TuxBlox Launcher " + std::string(version);
        choice = runMenu(title, options);

        if (options[choice] == "Settings") {
            settingsMenu();
            continue; // back to main menu after settings
        } else {
            break; // any other option exits the loop
        }
    }

    endwin(); // close ncurses mode

    if (options[choice] == "Launch Game") {
        printf("Launching game...\n");
        // system("./your_game_binary");
    } else if (options[choice] == "Check for Updates") {
        printf("Checking for updates...\n");
    } else if (options[choice] == "Quit") {
        printf("Quitting!\n");
    }

    return 0;
}