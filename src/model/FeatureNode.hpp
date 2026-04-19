#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace model {

/**
 * @brief Supported parametric feature types in Phase 2.
 */
enum class FeatureType {
    PartContainer,
    SketchFeature,
    Point,
    Line,
    Plane,
    ExtrudeFeature,
    RevolveFeature,
    FilletFeature,
    ChamferFeature,
    ShellFeature,
    HoleFeature,
    MirrorFeature
};

/**
 * @brief Feature validation and execution state.
 */
enum class FeatureState {
    Valid,
    Warning,
    Error,
    Suppressed
};

/**
 * @brief Geometric point object carried by Point feature nodes.
 */
struct PointObject {
    double x = 0.0;
    double y = 0.0;
    bool construction = false;
};

/**
 * @brief Geometric line object carried by Line feature nodes.
 */
struct LineObject {
    uint32_t point_a = 0U;
    uint32_t point_b = 0U;
    bool construction = false;
};

/**
 * @brief Plane object carried by Plane feature nodes.
 */
struct PlaneObject {
    uint32_t origin_point = 0U;
    uint32_t vector_ref = 0U;
};

using FeatureObjectData = std::variant<std::monostate, PointObject, LineObject, PlaneObject>;

/**
 * @brief Node in parametric feature tree.
 */
struct FeatureNode {
    uint32_t id = 0U;
    FeatureType type = FeatureType::SketchFeature;
    std::string name{};
    FeatureState state = FeatureState::Valid;
    bool suppressed = false;
    bool expanded = true;
    FeatureObjectData object_data{};
    FeatureNode* parent = nullptr;
    std::vector<FeatureNode*> children{};
};

}  // namespace model
