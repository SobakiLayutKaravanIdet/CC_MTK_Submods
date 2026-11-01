#pragma once
#include "V2ModAPI.h"

// Installs hooks at the province-only text builder and province render call.
// Country-label and GUI font paths are deliberately left untouched.
bool map_install_province_ttf_hooks(const V2ModAPI* api);
