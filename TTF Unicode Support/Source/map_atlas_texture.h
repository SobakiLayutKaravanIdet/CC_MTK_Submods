#pragma once
#include <d3d9.h>
#include "map_ttf_atlas.h"

class MapAtlasTexture {
public:
    ~MapAtlasTexture() { reset(); }
    MapAtlasTexture() = default;
    MapAtlasTexture(const MapAtlasTexture&) = delete;
    MapAtlasTexture& operator=(const MapAtlasTexture&) = delete;
    bool update(IDirect3DDevice9* device, const MapTtfAtlas& atlas);
    void reset();
    IDirect3DTexture9* get() const { return texture_; }
private:
    IDirect3DTexture9* texture_ = nullptr;
    IDirect3DDevice9* device_ = nullptr;
    unsigned width_ = 0, height_ = 0;
    uint64_t generation_ = 0;
};
