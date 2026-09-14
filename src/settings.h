#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "settings_core.h"

void load_settings();
void save_settings();
void show_settings_dialog(HWND parent);
void show_convert_quotes_dialog(HWND parent);
void show_about_dialog(HWND parent);
