#pragma once

#include <optional>
#include <string>

#include <imgui.h>
#include <glm/glm.hpp>

#include "sketch/SketchDocument.hpp"

namespace sketch {

/**
 * @brief Overlay renderer and lightweight interaction layer for sketch editing.
 */
class SketchView {
public:
    /**
     * @brief Requests opening plane properties window on next draw.
     */
    void request_open_plane_properties();

    /**
     * @brief Draws sketch entities and handles simple editing interactions.
     * @param document Sketch document.
     * @param viewport_origin Top-left viewport position in screen coordinates.
     * @param viewport_size Viewport size in pixels.
     * @param view_projection Current view-projection matrix.
     */
    void draw_overlay(
        SketchDocument& document,
        const ImVec2& viewport_origin,
        const ImVec2& viewport_size,
        const glm::mat4& view_projection);

private:
    struct SnapResult {
        glm::vec2 world{0.0f, 0.0f};
        bool snapped_to_grid = false;
        bool snapped_to_entity = false;
    };

    [[nodiscard]] ImVec2 world_to_screen(
        const ImVec2& origin,
        const ImVec2& size,
        const glm::mat4& view_projection,
        const Plane& plane,
        const glm::vec2& world_mm) const;
    [[nodiscard]] std::optional<glm::vec2> screen_to_sketch_mm(
        const ImVec2& origin,
        const ImVec2& size,
        const glm::mat4& view_projection,
        const Plane& plane,
        const ImVec2& screen) const;
    [[nodiscard]] SnapResult compute_snap(
        const SketchDocument& document,
        const ImVec2& origin,
        const ImVec2& size,
        const glm::mat4& view_projection,
        const ImVec2& mouse) const;

    std::optional<glm::vec2> pending_line_start_{};
    bool show_plane_properties_ = false;
};

}  // namespace sketch
