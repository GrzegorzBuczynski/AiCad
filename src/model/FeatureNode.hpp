#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace model {

/**
 * @brief Supported parametric feature types in Phase 2.
 */
enum class FeatureType {
    PartContainer,
    SketchFeature,
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
 * @brief Node in parametric feature tree.
 */
struct FeatureNode {
    uint32_t id = 0U;
    FeatureType type = FeatureType::SketchFeature;
    std::string name{};
    FeatureState state = FeatureState::Valid;
    bool suppressed = false;
    bool expanded = true;
    FeatureNode* parent = nullptr;
    std::vector<FeatureNode*> children{};
};

}  // namespace model
