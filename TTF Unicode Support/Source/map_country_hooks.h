#pragma once
#include "V2ModAPI.h"

// Installs the country-label-only Unicode/TTF path. The hook is deliberately
// version-locked to Victoria II 3.04 and validates each patch site.
bool map_install_country_hooks(const V2ModAPI* api);
