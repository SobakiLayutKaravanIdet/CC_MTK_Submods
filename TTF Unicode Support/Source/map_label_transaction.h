#pragma once
#include "map_country_pipeline.h"

// Per-render-thread staging, independent from engine memory. A captured input
// must remain alive until both construction and drawing have completed.
class MapLabelTransaction {
public:
    bool begin(MapCountryResources& resources, const char* utf8, bool use_small);
    bool begin_game_string(MapCountryResources& resources, const void* game_string, bool use_small);
    bool add_tracking(unsigned count, float space_advance);
    bool place_and_curve(MapGameCurveFn curve, const MapCurveArguments& args,
                         const float origin[3], const float horizontal[3],
                         const float vertical[3], float scale);
    bool commit(MapVertex* destination, size_t capacity) const;
    void clear();
    const MapCountryLabel* staged() const { return state_ == Ready ? &label_ : nullptr; }
    const short* measured() const { return state_ == Empty ? nullptr : label_.measured; }
private:
    enum State { Empty, Prepared, Ready };
    State state_ = Empty;
    MapCountryLabel label_;
};
