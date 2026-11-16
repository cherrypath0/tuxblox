* Added improvements for handling and repairing the virtual drive
* Added the flag `tbxFlagTranslateWindowsShortcuts`, on by default, which translates Roblox's Windows-formatted desktop shortcuts to Linux desktop shortcuts
* Added session identifiers for each Roblox instance
* Changed the way Roblox renders pixels to match Windows
* Changed the check that Roblox's files are genuine to run as Roblox starts, rather than in the launcher beforehand
* Changed the look of the message dialogs
* Fixed a crash corrupting the entire virtual drive and requiring a reset
* Fixed issues with message dialogs
* Fixed closing one instance of Roblox waiting for the others to close
* Fixed closing one instance of Roblox automatically focusing to the other ones
* Fixed focus not working whenever a file manager is opened by Roblox
* Fixed menus and title bars incorrectly doubling in size at higher desktop scales
* Fixed Roblox freezing forever whenever it raises a dialog
* Fixed the mouse lock sometimes not locking properly whenever focusing back to Roblox
* Fixed the integrated file manager appearing a minute after the native file manager appears
* Fixed TuxBlox refusing to launch some official Roblox builds that were signed differently
* Fixed new windows randomly stealing focus
* Fixed Roblox being unable to flash in the taskbar
* Fixed Studio camera issues on GNOME XWayland
* Fixed Studio freezing when a built-in web panel stopped loading
* Fixed Studio freezing when opening a file picker
* Fixed files saved into nested directories ending up in the wrong place
* Fixed file pickers opening on internal paths instead of the real folders
* Fixed Roblox failing to clear its cache
* Renamed TuxBlox's server process to "tuxblox-server"
* Removed two uninformative session log lines
* Removed uninformative window-related web engine logs