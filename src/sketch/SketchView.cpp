#include "sketch/SketchView.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <imgui.h>
#include <glm/gtc/matrix_inverse.hpp>

namespace sketch {

namespace {

constexpr float k_snap_pixels = 5.0f;
constexpr float k_mm_to_world = 0.02f;
constexpr float k_world_to_mm = 1.0f / k_mm_to_world;

struct PlaneBasis {
    glm::vec3 u{1.0f, 0.0f, 0.0f};
    glm::vec3 v{0.0f, 1.0f, 0.0f};
};

PlaneBasis make_basis(const glm::vec3& normal) {
    const glm::vec3 safe_normal = glm::normalize(glm::length(normal) < 1.0e-6f ? glm::vec3{0.0f, 0.0f, 1.0f} : normal);
    const glm::vec3 helper = std::abs(safe_normal.z) > 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{0.0f, 0.0f, 1.0f};
    const glm::vec3 u = glm::normalize(glm::cross(helper, safe_normal));
    const glm::vec3 v = glm::normalize(glm::cross(safe_normal, u));
    return {u, v};
}

glm::vec3 mm_to_world(const Plane& plane, const PlaneBasis& basis, const glm::vec2& point_mm) {
    return plane.origin + basis.u * (point_mm.x * k_mm_to_world) + basis.v * (point_mm.y * k_mm_to_world);
}

glm::vec2 world_to_mm(const Plane& plane, const PlaneBasis& basis, const glm::vec3& point_world) {
    const glm::vec3 offset = point_world - plane.origin;
    return {
        glm::dot(offset, basis.u) * k_world_to_mm,
        glm::dot(offset, basis.v) * k_world_to_mm,
    };
}

glm::vec2 closest_point_on_segment(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b) {
    const glm::vec2 ab = b - a;
    const float ab_len2 = glm::dot(ab, ab);
    if (ab_len2 <= 1.0e-8f) {
        return a;
    }

    const float t = std::clamp(glm::dot(p - a, ab) / ab_len2, 0.0f, 1.0f);
    return a + ab * t;
}

}  // namespace

ImVec2 SketchView::world_to_screen(
    const ImVec2& origin,
    const ImVec2& size,
    const glm::mat4& view_projection,
    const Plane& plane,
    const glm::vec2& world_mm) const {
    const PlaneBasis basis = make_basis(plane.normal);
    const glm::vec4 clip = view_projection * glm::vec4(mm_to_world(plane, basis, world_mm), 1.0f);
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
    const Plane& plane,
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

    const glm::vec3 normal = glm::normalize(glm::length(plane.normal) < 1.0e-6f ? glm::vec3{0.0f, 0.0f, 1.0f} : plane.normal);

    const float denom = glm::dot(normal, ray);
    if (std::abs(denom) < 1.0e-6f) {
        return std::nullopt;
    }

    const float t = glm::dot(normal, plane.origin - near_world) / denom;
    const glm::vec3 hit = near_world + ray * t;
    const PlaneBasis basis = make_basis(normal);
    return world_to_mm(plane, basis, hit);
}

SketchView::SnapResult SketchView::compute_snap(
    const SketchDocument& document,
    const ImVec2& origin,
    const ImVec2& size,
    const glm::mat4& view_projection,
    const ImVec2& mouse,
    bool allow_snap) const {
    SnapResult result{};
    const Plane& plane = document.plane();
    const auto mouse_mm = screen_to_sketch_mm(origin, size, view_projection, plane, mouse);
    if (!mouse_mm.has_value()) {
        return result;
    }

    result.world = *mouse_mm;

    const auto snap_priority = [](SnapKind kind) {
        switch (kind) {
        case SnapKind::Endpoint:
            return 0;
        case SnapKind::Segment:
            return 1;
        case SnapKind::External:
            return 2;
        case SnapKind::Grid:
            return 3;
        case SnapKind::None:
        default:
            return 99;
        }
    };

    int best_priority = snap_priority(SnapKind::None);
    float best_dist = std::numeric_limits<float>::max();
    const auto try_snap_candidate = [&](const glm::vec2& candidate, SnapKind kind) {
        const ImVec2 candidate_screen = world_to_screen(origin, size, view_projection, plane, candidate);
        const float d = std::hypot(mouse.x - candidate_screen.x, mouse.y - candidate_screen.y);
        if (d > k_snap_pixels) {
            return;
        }

        const int candidate_priority = snap_priority(kind);
        if (candidate_priority < best_priority || (candidate_priority == best_priority && d < best_dist)) {
            best_priority = candidate_priority;
            best_dist = d;
            result.world = candidate;
            result.kind = kind;
        }
    };

    if (allow_snap && snap_to_grid_) {
        float minor_step = 1.0f;
        if (const GridFeature* grid = document.active_grid_feature()) {
            minor_step = std::max(grid->minor_step_mm, 0.001f);
        }

        glm::vec2 snapped_grid = result.world;
        snapped_grid.x = std::round(snapped_grid.x / minor_step) * minor_step;
        snapped_grid.y = std::round(snapped_grid.y / minor_step) * minor_step;

        try_snap_candidate(snapped_grid, SnapKind::Grid);
    }

    for (const SketchEntity& entity : document.entities()) {
        if (allow_snap && snap_to_endpoints_) {
            const std::vector<glm::vec2> points = control_points(entity);
            for (const glm::vec2& point : points) {
                try_snap_candidate(point, SnapKind::Endpoint);
            }
        }

        if (allow_snap && snap_to_segments_) {
            if (const auto* line = std::get_if<LineEntity>(&entity.data)) {
            const glm::vec2 candidate = closest_point_on_segment(result.world, line->p1, line->p2);
            try_snap_candidate(candidate, SnapKind::Segment);
            }
        }
    }

    if (allow_snap && snap_to_external_ && !external_snap_edges_.empty()) {
        const PlaneBasis basis = make_basis(plane.normal);
        for (const geometry::EdgePolyline& polyline : external_snap_edges_) {
            if (polyline.size() < 2U) {
                continue;
            }

            for (size_t i = 1; i < polyline.size(); ++i) {
                const glm::vec2 a = world_to_mm(plane, basis, polyline[i - 1U]);
                const glm::vec2 b = world_to_mm(plane, basis, polyline[i]);
                const glm::vec2 candidate = closest_point_on_segment(result.world, a, b);
                try_snap_candidate(candidate, SnapKind::External);
            }
        }
    }

    return result;
}

void SketchView::request_open_plane_properties() {
    show_plane_properties_ = true;
    request_sync_plane_editor_from_document();
}

void SketchView::request_sync_plane_editor_from_document() {
    sync_plane_editor_from_document_ = true;
}

bool SketchView::blocks_3d_picking() const {
    return active_tool_ == Tool::Line;
}

void SketchView::set_external_snap_edges(const geometry::EdgePolylines& edges) {
    external_snap_edges_ = edges;
}

void SketchView::draw_toolbar(SketchDocument& document, const ImVec2& viewport_origin) {
    if (!document.is_active()) {
        return;
    }

    if (!toolbar_window_initialized_) {
        ImGui::SetNextWindowPos(ImVec2(viewport_origin.x + 16.0f, viewport_origin.y + 16.0f), ImGuiCond_FirstUseEver);
        toolbar_window_initialized_ = true;
    }

    constexpr ImGuiWindowFlags toolbar_flags = ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Sketch Toolbar", nullptr, toolbar_flags);

    const bool select_tool = active_tool_ == Tool::Select;
    if (ImGui::Button(select_tool ? "\xE2\x8C\x96 Select Tool [active]" : "\xE2\x8C\x96 Select Tool")) {
        active_tool_ = Tool::Select;
        pending_line_start_.reset();
    }
    ImGui::SameLine();

    const bool line_tool = active_tool_ == Tool::Line;
    if (ImGui::Button(line_tool ? "\xE2\x9C\x8E Add Line [active]" : "\xE2\x9C\x8E Add Line")) {
        active_tool_ = Tool::Line;
    }
    ImGui::SameLine();

    if (active_tool_ == Tool::Line && pending_line_start_.has_value()) {
        if (ImGui::Button("\xE2\x9C\x95 Cancel Segment")) {
            pending_line_start_.reset();
        }
        ImGui::SameLine();
    }

    bool snap_state = document.snap_enabled();
    if (ImGui::Checkbox("Snap", &snap_state)) {
        document.set_snap_enabled(snap_state);
    }
    ImGui::SameLine();
    ImGui::TextDisabled(ImGui::GetIO().KeyShift ? "Shift: snap OFF" : "Shift: hold to disable snap");

    ImGui::Separator();
    ImGui::TextUnformatted("Snap to:");
    ImGui::Checkbox("Grid", &snap_to_grid_);
    ImGui::SameLine();
    ImGui::Checkbox("End", &snap_to_endpoints_);
    ImGui::SameLine();
    ImGui::Checkbox("Seg", &snap_to_segments_);
    ImGui::SameLine();
    ImGui::Checkbox("Ext", &snap_to_external_);

    ImGui::Separator();
    if (!document.has_grid_feature()) {
        if (ImGui::Button("\xE2\x96\xA6 Add Grid Feature")) {
            document.add_grid_feature_on_plane();
        }
    } else {
        ImGui::TextUnformatted("Grid feature active");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Plane: Plane.001");
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        show_plane_properties_ = true;
        request_sync_plane_editor_from_document();
    }

    ImGui::TextDisabled(active_tool_ == Tool::Line
        ? "Line: LMB start/end segment, ESC close + rebuild"
        : "Select: 3D pick enabled, ESC close + rebuild");

    ImGui::End();
}

void SketchView::draw_overlay(
    SketchDocument& document,
    const ImVec2& viewport_origin,
    const ImVec2& viewport_size,
    const glm::mat4& view_projection) {
    if (document.is_active() && viewport_size.x > 1.0f && viewport_size.y > 1.0f) {
        ImDrawList* draw_list = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
        const ImVec2 end{viewport_origin.x + viewport_size.x, viewport_origin.y + viewport_size.y};
        const ImVec2 mouse = ImGui::GetMousePos();
        const bool shift_pressed = ImGui::GetIO().KeyShift;
        const bool allow_snap = document.snap_enabled() && !shift_pressed;
        const bool viewport_hovered =
            mouse.x >= viewport_origin.x && mouse.x <= end.x &&
            mouse.y >= viewport_origin.y && mouse.y <= end.y;

        draw_list->PushClipRect(viewport_origin, end, true);

        const SnapResult snap = compute_snap(document, viewport_origin, viewport_size, view_projection, mouse, allow_snap);
        const Plane& plane = document.plane();
        const ImVec2 snap_screen = world_to_screen(viewport_origin, viewport_size, view_projection, plane, snap.world);
        ImU32 snap_color = IM_COL32(245, 140, 20, 255);
        const char* snap_label = "free";
        switch (snap.kind) {
        case SnapKind::Endpoint:
            snap_color = IM_COL32(46, 160, 80, 255);
            snap_label = "endpoint";
            break;
        case SnapKind::Segment:
            snap_color = IM_COL32(58, 120, 210, 255);
            snap_label = "segment";
            break;
        case SnapKind::External:
            snap_color = IM_COL32(142, 96, 212, 255);
            snap_label = "external";
            break;
        case SnapKind::Grid:
            snap_color = IM_COL32(196, 130, 24, 255);
            snap_label = "grid";
            break;
        case SnapKind::None:
        default:
            break;
        }

        draw_list->AddCircle(snap_screen, 4.5f, snap_color, 20, 1.5f);
        if (snap.kind != SnapKind::None) {
            draw_list->AddCircleFilled(snap_screen, 2.2f, snap_color, 12);
        }
        draw_list->AddText(ImVec2(snap_screen.x + 8.0f, snap_screen.y - 16.0f), snap_color, snap_label);

        if (active_tool_ == Tool::Line && viewport_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
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

        if (active_tool_ == Tool::Line && pending_line_start_.has_value() && viewport_hovered) {
            const ImVec2 start = world_to_screen(viewport_origin, viewport_size, view_projection, plane, *pending_line_start_);
            draw_list->AddLine(start, snap_screen, IM_COL32(245, 140, 20, 255), 1.5f);
        }
        draw_list->PopClipRect();
    }

    draw_toolbar(document, viewport_origin);

    if (show_plane_properties_) {
        if (sync_plane_editor_from_document_) {
            const Plane& plane = document.plane();
            plane_origin_edit_ = plane.origin;
            plane_normal_edit_ = plane.normal;
            sync_plane_editor_from_document_ = false;
        }

        ImGui::SetNextWindowSize(ImVec2(440.0f, 280.0f), ImGuiCond_FirstUseEver);
        bool keep_open = true;
        if (ImGui::Begin("Plane Properties", &keep_open)) {
            ImGui::DragFloat3("Origin", &plane_origin_edit_.x, 0.1f, -10000.0f, 10000.0f, "%.3f");
            ImGui::DragFloat3("Normal", &plane_normal_edit_.x, 0.01f, -1.0f, 1.0f, "%.3f");
            if (ImGui::Button("Apply Plane")) {
                document.set_plane(Plane{plane_origin_edit_, plane_normal_edit_});
            }

            if (GridFeature* grid = document.active_grid_feature()) {
                ImGui::Separator();
                ImGui::TextUnformatted("Grid Feature");
                ImGui::Checkbox("Visible", &grid->visible);
                ImGui::DragFloat("Minor step (mm)", &grid->minor_step_mm, 0.05f, 0.1f, 100.0f, "%.2f");
                ImGui::DragFloat("Major step (mm)", &grid->major_step_mm, 0.1f, 1.0f, 1000.0f, "%.2f");
                ImGui::DragInt("Half cells", &grid->half_cells, 1.0f, 10, 400);
                grid->minor_step_mm = std::max(grid->minor_step_mm, 0.1f);
                grid->major_step_mm = std::max(grid->major_step_mm, grid->minor_step_mm);
            }
        }
        ImGui::End();
        show_plane_properties_ = keep_open;
    }

}

}  // namespace sketch
