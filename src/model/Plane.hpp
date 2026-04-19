#pragma once

#include <glm/glm.hpp>

namespace model {

/**
 * @brief Generic editable plane in 3D space.
 */
struct Plane {
    explicit Plane(const glm::vec3& feature_point, const glm::vec3& plane_normal = {0.0f, 0.0f, 1.0f})
        : origin(feature_point), normal(plane_normal) {}

    Plane() = delete;

    glm::vec3 origin;
    glm::vec3 normal;
};

}  // namespace model
