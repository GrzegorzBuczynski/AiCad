#pragma once

#include <cstdint>

#include "model/Plane.hpp"

namespace sketch {

using Plane = model::Plane;

/**
 * @brief Grid feature attached to a plane.
 */
struct GridFeature {
    uint32_t id = 0U;
    Plane plane;
    float minor_step_mm = 1.0f;
    float major_step_mm = 10.0f;
    int half_cells = 60;
    bool visible = true;
};

}  // namespace sketch
