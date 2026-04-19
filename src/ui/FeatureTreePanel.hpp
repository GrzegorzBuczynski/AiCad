#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "model/FeatureTree.hpp"
#include "ui/Panel.hpp"

namespace ui {

/**
 * @brief Rebuild intent emitted by feature tree interactions.
 */
struct RebuildIntent {
    bool full_rebuild = false;
    uint32_t start_node_id = 0U;
};

/**
 * @brief Left dock panel showing feature hierarchy.
 */
class FeatureTreePanel final : public Panel {
public:
    /**
     * @brief Sets bound feature tree model.
     * @param tree Tree model pointer.
     */
    void set_feature_tree(model::FeatureTree* tree);

    /**
     * @brief Draws the feature tree panel.
     */
    void draw() override;

    /**
     * @brief Consumes pending rebuild intent.
     * @return Pending rebuild intent when available.
     */
    std::optional<RebuildIntent> consume_rebuild_intent();

    /**
     * @brief Consumes pending sketch-open request emitted by double click.
     * @return True when a sketch node requested opening sketch mode.
     */
    bool consume_open_sketch_request();

    /**
     * @brief Consumes pending plane-properties request emitted by double click.
     * @return True when a plane node requested opening plane properties.
     */
    bool consume_open_plane_properties_request();

    /**
     * @brief Sets panel-local font scale.
     * @param scale Requested scale factor.
     */
    void set_font_scale(float scale);

private:
    void draw_node(model::FeatureNode* node);
    [[nodiscard]] static const char* icon_for_type(model::FeatureType type);
    [[nodiscard]] static uint32_t color_for_state(model::FeatureState state);

    model::FeatureTree* tree_ = nullptr;
    std::optional<RebuildIntent> rebuild_intent_{};
    uint32_t rename_node_id_ = 0U;
    std::array<char, 128> rename_buffer_{};
    bool open_sketch_request_ = false;
    bool open_plane_properties_request_ = false;
    float font_scale_ = 1.0f;
};

}  // namespace ui
