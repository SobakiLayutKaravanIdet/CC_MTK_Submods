#pragma once
#include "map_atlas_texture.h"
#include "map_country_bridge.h"

struct MapCountryLabel {
    std::string mask;
    std::vector<MapVertex> vertices;
    float advance = 0;
    short measured[2] = {};
    IDirect3DTexture9* texture = nullptr; // borrowed until resources are destroyed
};

// Adds spaces between Unicode code points before the game curves the mesh.
bool map_add_tracking(MapCountryLabel& label, unsigned spaces_between, float space_advance);

class MapCountryResources {
public:
    bool initialize(IDirect3DDevice9* device, const wchar_t* font_path, const wchar_t* reserve_font_path,
                    int large_height = 56, int small_height = 22);
    bool build(const char* utf8, bool use_small, MapCountryLabel& out);
    bool space_advance(bool use_small, float& out);
    IDirect3DTexture9* texture(bool use_small) const {
        return use_small ? small_texture_.get() : large_texture_.get();
    }
private:
    IDirect3DDevice9* device_ = nullptr; // device lifetime is kept by each texture once created
    MapTtfAtlas large_, small_;
    MapAtlasTexture large_texture_, small_texture_;
};
