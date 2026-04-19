#pragma once

#include <array>
#include <optional>
#include <glm/vec2.hpp>
#include <string>
#include <vector>

#include "model/FeatureTree.hpp"
#include "ui/Panel.hpp"

namespace sketch {
class SketchDocument;
}

namespace ui {

/**
 * @brief Rebuild intent emitted by feature tree interactions.
 */
struct RebuildIntent {
    bool full_rebuild = false;
    uint32_t start_feature_id = 0U;
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
     * @brief Sets bound sketch document for entity hierarchy visualization.
     * @param document Sketch document pointer.
     */
    void set_sketch_document(sketch::SketchDocument* document);

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
    struct PendingDragMove {
        uint32_t dragged_id = 0U;
        uint32_t target_parent_id = 0U;
        size_t insert_index = 0U;
    };

    void draw_feature_node(model::FeatureNode* feature);
    [[nodiscard]] static const char* icon_for_type(model::FeatureType type);
    [[nodiscard]] static uint32_t color_for_state(model::FeatureState state);
    void draw_sketch_entity_hierarchy();
    void apply_pending_drag_move();

    model::FeatureTree* tree_ = nullptr;
    sketch::SketchDocument* sketch_document_ = nullptr;
    std::optional<RebuildIntent> rebuild_intent_{};
    std::optional<PendingDragMove> pending_drag_move_{};
    uint32_t rename_feature_id_ = 0U;
    std::array<char, 128> rename_buffer_{};
    uint32_t edit_point_id_ = 0U;
    glm::vec2 edit_point_value_{0.0f, 0.0f};
    bool open_sketch_request_ = false;
    bool open_plane_properties_request_ = false;
    float font_scale_ = 1.0f;
};

}  // namespace ui
