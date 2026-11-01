#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <map>
#include <set>
#include <vector>
#include <string>

struct MapGlyph {
    unsigned x, y, width, height;
    int left, top, advance;
};

bool map_prepare_label(const char* utf8, std::string& uppercase_utf8, std::string& curve_mask);

struct MapVertex { float x, y, z, u, v; };
static_assert(sizeof(MapVertex) == 20, "Victoria II map vertex stride must be 20 bytes");
bool map_place_vertices(std::vector<MapVertex>& vertices, float advance, const float origin[3], const float horizontal[3], const float vertical[3], float scale);

class MapTtfAtlas {
public:
    MapTtfAtlas() = default;
    ~MapTtfAtlas();
    MapTtfAtlas(const MapTtfAtlas&) = delete;
    MapTtfAtlas& operator=(const MapTtfAtlas&) = delete;
    bool initialize(const wchar_t* file, const wchar_t* reserve_file, int pixels, unsigned width, unsigned height);
    bool glyph(wchar_t character, MapGlyph& out);
    bool layout_utf8(const char* text, std::vector<MapVertex>& vertices, float& advance);
    bool build_country_label(const char* text, std::string& curve_mask, std::vector<MapVertex>& vertices, float& advance);
    bool measure_country_label(const char* text, int padding_x, int padding_y, short (&result)[2]);
    const std::vector<uint8_t>& pixels() const { return pixels_; }
    unsigned width() const { return width_; }
    unsigned height() const { return height_; }
    uint64_t generation() const { return generation_; }
private:
    HDC dc_ = nullptr;
    HFONT font_ = nullptr;
    HGDIOBJ old_ = nullptr;
    HANDLE font_resource_ = nullptr;
    HDC reserve_dc_ = nullptr;
    HFONT reserve_font_ = nullptr;
    HGDIOBJ reserve_old_ = nullptr;
    HANDLE reserve_font_resource_ = nullptr;
    std::map<wchar_t, MapGlyph> glyphs_;
    std::set<wchar_t> reserve_glyphs_;
    std::map<uint32_t, int> kerning_;
    std::vector<uint8_t> pixels_;
    unsigned width_ = 0, height_ = 0, x_ = 1, y_ = 1, row_ = 0;
    uint64_t generation_ = 0;
};
