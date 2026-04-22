#include "ui/ViewportPanel.hpp"

#include <imgui.h>

namespace ui {

namespace {

struct ScreenPoint {
    bool visible = false;
    ImVec2 pos{};
};

ScreenPoint project_point(const glm::mat4& view_projection, const ImVec2& origin, const ImVec2& size, const glm::vec3& point) {
    const glm::vec4 clip = view_projection * glm::vec4(point, 1.0f);
    if (std::abs(clip.w) < 1.0e-5f) {
        return {};
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.0f || ndc.z > 1.0f) {
        return {};
    }

    const float x = origin.x + (ndc.x * 0.5f + 0.5f) * size.x;
    const float y = origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * size.y;
    return {true, ImVec2(x, y)};
}

void draw_segment(ImDrawList* draw_list, const glm::mat4& view_projection, const ImVec2& origin, const ImVec2& size, const glm::vec3& a, const glm::vec3& b, ImU32 color, float thickness = 1.5f) {
    const ScreenPoint start = project_point(view_projection, origin, size, a);
    const ScreenPoint end = project_point(view_projection, origin, size, b);
    if (start.visible && end.visible) {
        draw_list->AddLine(start.pos, end.pos, color, thickness);
    }
}

void draw_sketch_profiles(
    ImDrawList* draw_list,
    const glm::mat4& view_projection,
    const ImVec2& origin,
    const ImVec2& size,
    const std::vector<geometry::Profile>& profiles,
    const model::Plane& plane) {
    constexpr float k_mm_to_world = 0.02f;

    const glm::vec3 normal = glm::normalize(glm::length(plane.normal) < 1.0e-6f ? glm::vec3{0.0f, 0.0f, 1.0f} : plane.normal);
    const glm::vec3 helper = std::abs(normal.z) > 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{0.0f, 0.0f, 1.0f};
    const glm::vec3 u = glm::normalize(glm::cross(helper, normal));
    const glm::vec3 v = glm::normalize(glm::cross(normal, u));

    for (const geometry::Profile& profile : profiles) {
        if (profile.points.size() < 2U) {
            continue;
        }

        for (size_t i = 0; i < profile.points.size(); ++i) {
            const size_t next = (i + 1U) % profile.points.size();
            if (!profile.closed && next == 0U) {
                break;
            }

            const glm::vec2 a_mm = profile.points[i];
            const glm::vec2 b_mm = profile.points[next];

            const glm::vec3 a_world = plane.origin + u * (a_mm.x * k_mm_to_world) + v * (a_mm.y * k_mm_to_world);
            const glm::vec3 b_world = plane.origin + u * (b_mm.x * k_mm_to_world) + v * (b_mm.y * k_mm_to_world);
            draw_segment(draw_list, view_projection, origin, size, a_world, b_world, IM_COL32(40, 48, 64, 255), 2.2f);
        }
    }
}

void draw_selected_edges(
    ImDrawList* draw_list,
    const glm::mat4& view_projection,
    const ImVec2& origin,
    const ImVec2& size,
    const geometry::EdgePolylines& edges) {
    for (const geometry::EdgePolyline& polyline : edges) {
        if (polyline.size() < 2U) {
            continue;
        }

        for (size_t i = 1; i < polyline.size(); ++i) {
            draw_segment(
                draw_list,
                view_projection,
                origin,
                size,
                polyline[i - 1U],
                polyline[i],
                IM_COL32(245, 140, 20, 255),
                3.2f);
        }
    }
}

void draw_plane_direction_guides(
    ImDrawList* draw_list,
    const glm::mat4& view_projection,
    const ImVec2& origin,
    const ImVec2& size,
    const model::Plane& plane) {
    const glm::vec3 normal = glm::normalize(glm::length(plane.normal) < 1.0e-6f ? glm::vec3{0.0f, 0.0f, 1.0f} : plane.normal);
    const glm::vec3 helper = std::abs(normal.z) > 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{0.0f, 0.0f, 1.0f};
    const glm::vec3 u = glm::normalize(glm::cross(helper, normal));
    const glm::vec3 v = glm::normalize(glm::cross(normal, u));

    constexpr float k_axis_half_extent_world = 1.2f;  // 60mm in current sketch scale
    const glm::vec3 u_a = plane.origin - u * k_axis_half_extent_world;
    const glm::vec3 u_b = plane.origin + u * k_axis_half_extent_world;
    const glm::vec3 v_a = plane.origin - v * k_axis_half_extent_world;
    const glm::vec3 v_b = plane.origin + v * k_axis_half_extent_world;

    draw_segment(draw_list, view_projection, origin, size, u_a, u_b, IM_COL32(116, 132, 158, 220), 1.8f);
    draw_segment(draw_list, view_projection, origin, size, v_a, v_b, IM_COL32(116, 132, 158, 220), 1.8f);

    // Keep plane visualization close to previous look: fixed 60x40mm frame.
    constexpr float k_rect_half_w_world = 30.0f * 0.02f;
    constexpr float k_rect_half_h_world = 20.0f * 0.02f;
    const glm::vec3 p0 = plane.origin + u * (-k_rect_half_w_world) + v * (-k_rect_half_h_world);
    const glm::vec3 p1 = plane.origin + u * (k_rect_half_w_world) + v * (-k_rect_half_h_world);
    const glm::vec3 p2 = plane.origin + u * (k_rect_half_w_world) + v * (k_rect_half_h_world);
    const glm::vec3 p3 = plane.origin + u * (-k_rect_half_w_world) + v * (k_rect_half_h_world);

    draw_segment(draw_list, view_projection, origin, size, p0, p1, IM_COL32(58, 120, 210, 200), 2.0f);
    draw_segment(draw_list, view_projection, origin, size, p1, p2, IM_COL32(58, 120, 210, 200), 2.0f);
    draw_segment(draw_list, view_projection, origin, size, p2, p3, IM_COL32(58, 120, 210, 200), 2.0f);
    draw_segment(draw_list, view_projection, origin, size, p3, p0, IM_COL32(58, 120, 210, 200), 2.0f);
}

void draw_grid_feature(
    ImDrawList* draw_list,
    const glm::mat4& view_projection,
    const ImVec2& origin,
    const ImVec2& size,
    const std::optional<sketch::GridFeature>& grid_feature) {
    if (!grid_feature.has_value() || !grid_feature->visible) {
        return;
    }

    const sketch::GridFeature& grid = *grid_feature;
    const model::Plane& plane = grid.plane;
    const glm::vec3 normal = glm::normalize(glm::length(plane.normal) < 1.0e-6f ? glm::vec3{0.0f, 0.0f, 1.0f} : plane.normal);
    const glm::vec3 helper = std::abs(normal.z) > 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{0.0f, 0.0f, 1.0f};
    const glm::vec3 u = glm::normalize(glm::cross(helper, normal));
    const glm::vec3 v = glm::normalize(glm::cross(normal, u));

    const float minor = std::max(grid.minor_step_mm, 0.1f) * 0.02f;
    const float major = std::max(grid.major_step_mm, minor);
    const int half_cells = std::max(grid.half_cells, 1);

    const float extent = static_cast<float>(half_cells) * minor;
    for (int i = -half_cells; i <= half_cells; ++i) {
        const float offset = static_cast<float>(i) * minor;
        const float major_ratio = (major / 0.02f) > 0.0f ? std::abs(std::fmod(offset / 0.02f, major / 0.02f)) : 1.0f;
        const bool is_major = major_ratio < 0.01f || std::abs(major_ratio - (major / 0.02f)) < 0.01f;
        const ImU32 color = is_major ? IM_COL32(155, 165, 178, 170) : IM_COL32(205, 214, 224, 150);

        const glm::vec3 a = plane.origin + u * (-extent) + v * offset;
        const glm::vec3 b = plane.origin + u * extent + v * offset;
        draw_segment(draw_list, view_projection, origin, size, a, b, color, is_major ? 1.4f : 1.0f);

        const glm::vec3 c = plane.origin + u * offset + v * (-extent);
        const glm::vec3 d = plane.origin + u * offset + v * extent;
        draw_segment(draw_list, view_projection, origin, size, c, d, color, is_major ? 1.4f : 1.0f);
    }
}

}  // namespace

void ViewportPanel::set_texture(ImTextureID texture) {
    viewport_texture_ = texture;
}

void ViewportPanel::set_camera_matrices(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& view_projection) {
    view_ = view;
    projection_ = projection;
    view_projection_ = view_projection;
}

void ViewportPanel::draw() {
    constexpr ImGuiWindowFlags viewport_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("Viewport", nullptr, viewport_flags);
    ImGui::SetWindowFontScale(font_scale_);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    content_origin_ = ImGui::GetCursorScreenPos();
    content_size_ = avail;
    hovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (viewport_texture_ != static_cast<ImTextureID>(0) && avail.x > 1.0f && avail.y > 1.0f) {
        ImGui::Image(viewport_texture_, avail);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_plane_direction_guides(draw_list, view_projection_, content_origin_, avail, sketch_plane_);
        draw_grid_feature(draw_list, view_projection_, content_origin_, avail, grid_feature_);
        draw_sketch_profiles(draw_list, view_projection_, content_origin_, avail, sketch_profiles_, sketch_plane_);
        draw_selected_edges(draw_list, view_projection_, content_origin_, avail, selected_edges_);
    } else {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 end = ImVec2(origin.x + avail.x, origin.y + avail.y);

        draw_list->AddRectFilled(origin, end, IM_COL32(244, 247, 251, 255));
        draw_list->AddRect(origin, end, IM_COL32(170, 185, 205, 255));

        const ImVec2 center = ImVec2((origin.x + end.x) * 0.5f, (origin.y + end.y) * 0.5f);
        draw_list->AddLine(ImVec2(center.x - 20.0f, center.y), ImVec2(center.x + 20.0f, center.y), IM_COL32(58, 120, 210, 255), 2.0f);
        draw_list->AddLine(ImVec2(center.x, center.y - 20.0f), ImVec2(center.x, center.y + 20.0f), IM_COL32(58, 120, 210, 255), 2.0f);

        const glm::vec3 cube_min{-1.0f, -1.0f, -1.0f};
        const glm::vec3 cube_max{1.0f, 1.0f, 1.0f};
        const glm::vec3 p000{cube_min.x, cube_min.y, cube_min.z};
        const glm::vec3 p001{cube_min.x, cube_min.y, cube_max.z};
        const glm::vec3 p010{cube_min.x, cube_max.y, cube_min.z};
        const glm::vec3 p011{cube_min.x, cube_max.y, cube_max.z};
        const glm::vec3 p100{cube_max.x, cube_min.y, cube_min.z};
        const glm::vec3 p101{cube_max.x, cube_min.y, cube_max.z};
        const glm::vec3 p110{cube_max.x, cube_max.y, cube_min.z};
        const glm::vec3 p111{cube_max.x, cube_max.y, cube_max.z};

        draw_segment(draw_list, view_projection_, origin, avail, p000, p001, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p000, p010, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p000, p100, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p001, p011, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p001, p101, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p010, p011, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p010, p110, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p100, p101, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p100, p110, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p011, p111, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p101, p111, IM_COL32(120, 145, 175, 255));
        draw_segment(draw_list, view_projection_, origin, avail, p110, p111, IM_COL32(120, 145, 175, 255));

        draw_segment(draw_list, view_projection_, origin, avail, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{2.0f, 0.0f, 0.0f}, IM_COL32(220, 90, 90, 255), 2.0f);
        draw_segment(draw_list, view_projection_, origin, avail, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 2.0f, 0.0f}, IM_COL32(80, 170, 110, 255), 2.0f);
        draw_segment(draw_list, view_projection_, origin, avail, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 2.0f}, IM_COL32(58, 120, 210, 255), 2.0f);
        draw_plane_direction_guides(draw_list, view_projection_, origin, avail, sketch_plane_);
        draw_grid_feature(draw_list, view_projection_, origin, avail, grid_feature_);
        draw_sketch_profiles(draw_list, view_projection_, origin, avail, sketch_profiles_, sketch_plane_);
        draw_selected_edges(draw_list, view_projection_, origin, avail, selected_edges_);

        ImGui::SetCursorScreenPos(ImVec2(origin.x + 16.0f, origin.y + 16.0f));
        ImGui::TextUnformatted("Camera-driven viewport");
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 16.0f, origin.y + 34.0f));
        ImGui::TextDisabled("No offscreen image yet, but View/Projection is active");

        ImGui::SetCursorScreenPos(ImVec2(origin.x + 16.0f, origin.y + 58.0f));
        ImGui::Text("View matrix[0][0]: %.3f", view_[0][0]);
        ImGui::Text("Projection matrix[0][0]: %.3f", projection_[0][0]);
        ImGui::Text("VP matrix[0][0]: %.3f", view_projection_[0][0]);

    }

    ImGui::End();
}

ImVec2 ViewportPanel::content_origin() const {
    return content_origin_;
}

ImVec2 ViewportPanel::content_size() const {
    return content_size_;
}

bool ViewportPanel::is_hovered() const {
    return hovered_;
}

float ViewportPanel::estimate_pick_tolerance_world(ImVec2 mouse_pos, float pixels) const {
    if (content_size_.x <= 1.0f || content_size_.y <= 1.0f || pixels <= 0.0f) {
        return 0.01f;
    }

    const glm::mat4 view_projection = projection_ * view_;
    const glm::mat4 inverse_vp = glm::inverse(view_projection);

    // Use a representative depth (middle of view frustum) instead of near clip plane
    // This gives a more realistic world tolerance for typical geometry positions
    const float depth_z = 0.0f;  // NDC z=0 is middle of frustum

    const auto screen_to_world_at_depth = [&](ImVec2 screen, float z_ndc, glm::vec3* out_world) -> bool {
        const float local_x = (screen.x - content_origin_.x) / content_size_.x;
        const float local_y = (screen.y - content_origin_.y) / content_size_.y;
        if (local_x < 0.0f || local_x > 1.0f || local_y < 0.0f || local_y > 1.0f) {
            return false;
        }

        const float ndc_x = local_x * 2.0f - 1.0f;
        const float ndc_y = 1.0f - local_y * 2.0f;
        const glm::vec4 clip{ndc_x, ndc_y, z_ndc, 1.0f};
        const glm::vec4 world4 = inverse_vp * clip;
        if (std::abs(world4.w) < 1.0e-6f) {
            return false;
        }

        *out_world = glm::vec3(world4) / world4.w;
        return true;
    };

    glm::vec3 center_world{0.0f};
    glm::vec3 offset_world{0.0f};
    if (!screen_to_world_at_depth(mouse_pos, depth_z, &center_world)) {
        return 0.01f;
    }

    ImVec2 offset_mouse = mouse_pos;
    offset_mouse.x += pixels;
    if (!screen_to_world_at_depth(offset_mouse, depth_z, &offset_world)) {
        offset_mouse = mouse_pos;
        offset_mouse.y += pixels;
        if (!screen_to_world_at_depth(offset_mouse, depth_z, &offset_world)) {
            return 0.01f;
        }
    }

    return std::max(glm::length(offset_world - center_world), 0.0001f);
}

#if defined(VULCANCAD_HAS_OCCT)
std::optional<ViewportPanel::Ray> ViewportPanel::getClickRay(ImVec2 mouse_pos) const {
    if (content_size_.x <= 1.0f || content_size_.y <= 1.0f) {
        return std::nullopt;
    }

    const float local_x = (mouse_pos.x - content_origin_.x) / content_size_.x;
    const float local_y = (mouse_pos.y - content_origin_.y) / content_size_.y;
    if (local_x < 0.0f || local_x > 1.0f || local_y < 0.0f || local_y > 1.0f) {
        return std::nullopt;
    }

    const float ndc_x = local_x * 2.0f - 1.0f;
    const float ndc_y = 1.0f - local_y * 2.0f;

    const glm::mat4 view_projection = projection_ * view_;
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
    const glm::vec3 ray_vec = far_world - near_world;
    if (glm::length(ray_vec) < 1.0e-6f) {
        return std::nullopt;
    }

    const glm::vec3 ray_dir = glm::normalize(ray_vec);
    return Ray{
        gp_Pnt(
            static_cast<double>(near_world.x),
            static_cast<double>(near_world.y),
            static_cast<double>(near_world.z)),
        gp_Dir(
            static_cast<double>(ray_dir.x),
            static_cast<double>(ray_dir.y),
            static_cast<double>(ray_dir.z))};
}
#endif

void ViewportPanel::set_sketch_profiles(const std::vector<geometry::Profile>& profiles) {
    sketch_profiles_ = profiles;
}

void ViewportPanel::set_selected_edges(const geometry::EdgePolylines& edges) {
    selected_edges_ = edges;
}

void ViewportPanel::set_sketch_plane(const model::Plane& plane) {
    sketch_plane_ = plane;
}

void ViewportPanel::set_grid_feature(std::optional<sketch::GridFeature> feature) {
    grid_feature_ = std::move(feature);
}

void ViewportPanel::set_font_scale(float scale) {
    font_scale_ = std::max(scale, 0.7f);
}

}  // namespace ui
