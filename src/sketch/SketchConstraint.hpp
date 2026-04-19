#pragma once

#include <cstdint>
#include <variant>

#include <glm/glm.hpp>

namespace sketch {

using constraint_id = uint32_t;
using entity_id = uint32_t;

/**
 * @brief Coincident constraint between two entity points.
 */
struct Coincident {
    entity_id entity_a = 0U;
    uint32_t point_a = 0U;
    entity_id entity_b = 0U;
    uint32_t point_b = 0U;
};

/**
 * @brief Parallel relation between two lines.
 */
struct Parallel {
    entity_id line_a = 0U;
    entity_id line_b = 0U;
};

/**
 * @brief Perpendicular relation between two lines.
 */
struct Perpendicular {
    entity_id line_a = 0U;
    entity_id line_b = 0U;
};

/**
 * @brief Tangency relation between two entities.
 */
struct Tangent {
    entity_id entity_a = 0U;
    entity_id entity_b = 0U;
};

/**
 * @brief Equal-length relation between two line segments.
 */
struct EqualLength {
    entity_id line_a = 0U;
    entity_id line_b = 0U;
};

/**
 * @brief Fixed point anchor.
 */
struct FixedPoint {
    entity_id entity = 0U;
    uint32_t point = 0U;
    glm::vec2 value{0.0f, 0.0f};
};

/**
 * @brief Horizontal line constraint.
 */
struct HorizontalConstraint {
    entity_id line = 0U;
};

/**
 * @brief Vertical line constraint.
 */
struct VerticalConstraint {
    entity_id line = 0U;
};

/**
 * @brief Distance dimension.
 */
struct DistanceDim {
    entity_id entity_a = 0U;
    uint32_t point_a = 0U;
    entity_id entity_b = 0U;
    uint32_t point_b = 0U;
    float value = 10.0f;
};

/**
 * @brief Angle dimension in degrees.
 */
struct AngleDim {
    entity_id line_a = 0U;
    entity_id line_b = 0U;
    float degrees = 90.0f;
};

/**
 * @brief Radius dimension.
 */
struct RadiusDim {
    entity_id entity = 0U;
    float value = 10.0f;
};

using SketchConstraintData = std::variant<
    Coincident,
    Parallel,
    Perpendicular,
    Tangent,
    EqualLength,
    FixedPoint,
    HorizontalConstraint,
    VerticalConstraint,
    DistanceDim,
    AngleDim,
    RadiusDim>;

/**
 * @brief Generic sketch constraint record.
 */
struct SketchConstraint {
    constraint_id id = 0U;
    SketchConstraintData data{};
};

}  // namespace sketch
