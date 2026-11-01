#pragma once
#include "map_country_resources.h"

// Staged preparation only: never mutates engine state or game memory. The
// caller owns the final commit into the game's vertex buffer and texture pass.
bool map_prepare_game_country(MapCountryResources& resources, const char* utf8,
                              bool use_small, MapGameCurveFn curve,
                              const MapCurveArguments& curve_args,
                              const float origin[3], const float horizontal[3],
                              const float vertical[3], float scale,
                              MapCountryLabel& out);

// Commit only after all staging succeeds; spare vertices are degenerate.
bool map_commit_game_vertices(const MapCountryLabel& label, MapVertex* destination, size_t capacity);
