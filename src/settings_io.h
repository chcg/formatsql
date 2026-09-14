#pragma once
#include <string>
#include "settings_core.h"

// Cross-platform (no WinAPI) .ini-style settings export/import, shared by the
// NPP "Export/Import as INI..." buttons, the standalone CLI, and the WASM
// bindings. Uses the exact same section/key vocabulary as save_settings()/
// load_settings() in settings_dialog.cpp (the NPP plugin's own INI store),
// minus FormatOnSave (an editor behavior, not a formatting parameter).
std::string    settings_to_ini(const FormatSettings& s);
FormatSettings settings_from_ini(const std::string& ini_text);
