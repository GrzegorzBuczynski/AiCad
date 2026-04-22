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
    enum class Tool {
        Select,
        Line,
    };

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

    /**
     * @brief Returns whether current sketch tool should block 3D pick clicks.
     */
    [[nodiscard]] bool blocks_3d_picking() const;

    /**
     * @brief Sets additional world-space edge polylines used as snapping guides.
     * @param edges Picked geometry edges.
     */
    void set_external_snap_edges(const geometry::EdgePolylines& edges);

private:
    enum class SnapKind {
        None,
        Grid,
        Endpoint,
        Segment,
        External,
    };

    struct SnapResult {
        glm::vec2 world{0.0f, 0.0f};
        SnapKind kind = SnapKind::None;
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
        const ImVec2& mouse,
        bool allow_snap) const;

    void request_sync_plane_editor_from_document();
    void draw_toolbar(SketchDocument& document, const ImVec2& viewport_origin);

    std::optional<glm::vec2> pending_line_start_{};
    Tool active_tool_ = Tool::Select;
    geometry::EdgePolylines external_snap_edges_{};
    bool snap_to_grid_ = true;
    bool snap_to_endpoints_ = true;
    bool snap_to_segments_ = true;
    bool snap_to_external_ = true;
    bool toolbar_window_initialized_ = false;
    bool show_plane_properties_ = false;
    bool sync_plane_editor_from_document_ = true;
    glm::vec3 plane_origin_edit_{0.0f, 0.0f, 0.0f};
    glm::vec3 plane_normal_edit_{0.0f, 0.0f, 1.0f};
};

}  // namespace sketch
