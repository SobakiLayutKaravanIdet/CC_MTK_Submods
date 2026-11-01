#pragma once
#include <windows.h>
#include <cstdint>

// Installer for an x86 E8 rel32 call. Installation/removal must occur while
// the game render thread is not executing this call site.
class MapCallPatch {
public:
    ~MapCallPatch();
    MapCallPatch() = default;
    MapCallPatch(const MapCallPatch&) = delete;
    MapCallPatch& operator=(const MapCallPatch&) = delete;
    bool install(void* call_site, const uint8_t expected[5], void* replacement);
    bool remove();
    void* original_target() const { return original_target_; }
private:
    uint8_t* site_ = nullptr;
    uint8_t original_[5] = {};
    uint8_t installed_[5] = {};
    void* original_target_ = nullptr;
};
