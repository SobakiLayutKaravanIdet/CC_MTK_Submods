#pragma once
#include "map_ttf_atlas.h"

// Signature recovered at v2game.exe 0x007DC320. The caller owns the vertex
// buffer, string and all opaque context pointers; the function is __cdecl.
using MapGameCurveFn = void (__cdecl *)(void*, void*, const void*, void*, void*, MapVertex*, int,
                                       float, float, float, float, float, float, int, int);
struct MapCurveArguments {
    void* context1; void* context2; void* context4; void* context5;
    float x1, y1, x2, y2, origin_x, origin_y;
    int map_width, map_height;
};

// Invoke only on the render thread while the game-provided context is alive.
// Each non-space byte must correspond to precisely six vertices.
bool map_apply_game_curve(MapGameCurveFn curve, const std::string& mask,
                          std::vector<MapVertex>& vertices, const MapCurveArguments& args);
