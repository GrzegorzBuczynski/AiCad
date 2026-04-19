#include "sketch/SketchView.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui.h>
#include <glm/gtc/matrix_inverse.hpp>

namespace sketch {

namespace {

constexpr float k_snap_pixels = 5.0f;
constexpr float k_mm_to_world = 0.02f;
constexpr float k_world_to_mm = 1.0f / k_mm_to_world;

glm::vec3 mm_to_world(const glm::vec2& point_mm) {
    return {point_mm.x * k_mm_to_world, point_mm.y * k_mm_to_world, 0.0f};
}

glm::vec2 world_to_mm(const glm::vec3& point_world) {
    return {point_world.x * k_world_to_mm, point_world.y * k_world_to_mm};
}

}  // namespace

ImVec2 SketchView::world_to_screen(
    const ImVec2& origin,
    const ImVec2& size,
    const glm::mat4& view_projection,
    const glm::vec2& world_mm) const {
    const glm::vec4 clip = view_projection * glm::vec4(mm_to_world(world_mm), 1.0f);
    if (std::abs(clip.w) < 1.0e-5f) {
        return {-10000.0f, -10000.0f};
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return {
        origin.x + (ndc.x * 0.5f + 0.5f) * size.x,
        origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * size.y,
    };
}

std::optional<glm::vec2> SketchView::screen_to_sketch_mm(
    const ImVec2& origin,
    const ImVec2& size,
    const glm::mat4& view_projection,
    const ImVec2& screen) const {
    const float x = (screen.x - origin.x) / std::max(size.x, 1.0f);
    const float y = (screen.y - origin.y) / std::max(size.y, 1.0f);

    const float ndc_x = x * 2.0f - 1.0f;
    const float ndc_y = 1.0f - y * 2.0f;

    const glm::mat4 inverse_vp = glm::inverse(view_projection);
    const glm::vec4 near_clip{ndc_x, ndc_y, -1.0f, 1.0f};
    const glm::vec4 far_clip{ndc_x, ndc_y, 1.0f, 1.0f};

    const glm::vec4 near_world4 = inverse_vp * near_clip;
    const glm::vec4 far_world4 = inverse_vp * far_clip;
    if (std::abs(near_world4.w) < 1.0e-6f || std::abs(far_world4.w) < 1.0e-6f) {
        return std::nullopt;
    }

    const glm::vec3 near_world = glm::vec3(near_world4) / near_world4.w;
    const glm::vec3 far_world = glm::vec3(far_world4) / far_world4.w;
    const glm::vec3 ray = far_world - near_world;

    if (std::abs(ray.z) < 1.0e-6f) {
        return std::nullopt;
    }

    const float t = -near_world.z / ray.z;
    const glm::vec3 hit = near_world + ray * t;
    return world_to_mm(hit);
}

SketchView::SnapResult SketchView::compute_snap(
    const SketchDocument& document,
    const ImVec2& origin,
    const ImVec2& size,
    const glm::mat4& view_projection,
    const ImVec2& mouse) const {
    SnapResult result{};
    const auto mouse_mm = screen_to_sketch_mm(origin, size, view_projection, mouse);
    if (!mouse_mm.has_value()) {
        return result;
    }

    result.world = *mouse_mm;

    glm::vec2 snapped_grid = result.world;
    snapped_grid.x = std::round(snapped_grid.x);
    snapped_grid.y = std::round(snapped_grid.y);

    const ImVec2 grid_screen = world_to_screen(origin, size, view_projection, snapped_grid);
    const float grid_dist = std::hypot(mouse.x - grid_screen.x, mouse.y - grid_screen.y);
    if (grid_dist <= k_snap_pixels) {
        result.world = snapped_grid;
        result.snapped_to_grid = true;
    }

    float best_dist = k_snap_pixels;
    for (const SketchEntity& entity : document.entities()) {
        const std::vector<glm::vec2> points = control_points(entity);
        for (const glm::vec2& point : points) {
            const ImVec2 point_screen = world_to_screen(origin, size, view_projection, point);
            const float d = std::hypot(mouse.x - point_screen.x, mouse.y - point_screen.y);
            if (d <= best_dist) {
                best_dist = d;
                result.world = point;
                result.snapped_to_entity = true;
            }
        }
    }

    return result;
}

void SketchView::draw_overlay(
    SketchDocument& document,
    const ImVec2& viewport_origin,
    const ImVec2& viewport_size,
    const glm::mat4& view_projection) {
    if (!document.is_active() || viewport_size.x <= 1.0f || viewport_size.y <= 1.0f) {
        return;
    }

    ImDrawList* draw_list = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    const ImVec2 end{viewport_origin.x + viewport_size.x, viewport_origin.y + viewport_size.y};
    const ImVec2 mouse = ImGui::GetMousePos();
    const bool viewport_hovered =
        mouse.x >= viewport_origin.x && mouse.x <= end.x &&
        mouse.y >= viewport_origin.y && mouse.y <= end.y;

    draw_list->PushClipRect(viewport_origin, end, true);

    const SnapResult snap = compute_snap(document, viewport_origin, viewport_size, view_projection, mouse);
    const ImVec2 snap_screen = world_to_screen(viewport_origin, viewport_size, view_projection, snap.world);
    draw_list->AddCircle(snap_screen, 4.5f, IM_COL32(245, 140, 20, 255), 20, 1.5f);

    if (viewport_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
        if (!pending_line_start_.has_value()) {
            pending_line_start_ = snap.world;
        } else {
            const glm::vec2 start = *pending_line_start_;
            const glm::vec2 end_point = snap.world;
            if (glm::length(end_point - start) > 0.001f) {
                document.add_line(start, end_point, false);
                document.solve();
            }
            pending_line_start_.reset();
        }
    }

    if (pending_line_start_.has_value() && viewport_hovered) {
        const ImVec2 start = world_to_screen(viewport_origin, viewport_size, view_projection, *pending_line_start_);
        draw_list->AddLine(start, snap_screen, IM_COL32(245, 140, 20, 255), 1.5f);
    }

    const ImVec2 strip_min = ImVec2(viewport_origin.x + 8.0f, viewport_origin.y + 8.0f);
    const ImVec2 strip_max = ImVec2(viewport_origin.x + 410.0f, viewport_origin.y + 34.0f);
    draw_list->AddRectFilled(strip_min, strip_max, IM_COL32(242, 246, 251, 235), 4.0f);
    draw_list->AddRect(strip_min, strip_max, IM_COL32(164, 177, 196, 255), 4.0f);
    draw_list->AddText(
        ImVec2(viewport_origin.x + 14.0f, viewport_origin.y + 14.0f),
        IM_COL32(40, 45, 55, 255),
        "Sketch mode tools: LMB line on sketch plane, ESC = close + rebuild");

    draw_list->PopClipRect();
}

}  // namespace sketch
