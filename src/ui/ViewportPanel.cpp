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
    ImGui::Begin("Viewport");

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (viewport_texture_ != static_cast<ImTextureID>(0) && avail.x > 1.0f && avail.y > 1.0f) {
        ImGui::Image(viewport_texture_, avail);
    } else {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 end = ImVec2(origin.x + avail.x, origin.y + avail.y);

        draw_list->AddRectFilled(origin, end, IM_COL32(244, 247, 251, 255));
        draw_list->AddRect(origin, end, IM_COL32(170, 185, 205, 255));

        const float grid_step = 48.0f;
        for (float x = origin.x; x < end.x; x += grid_step) {
            draw_list->AddLine(ImVec2(x, origin.y), ImVec2(x, end.y), IM_COL32(214, 223, 234, 255));
        }
        for (float y = origin.y; y < end.y; y += grid_step) {
            draw_list->AddLine(ImVec2(origin.x, y), ImVec2(end.x, y), IM_COL32(214, 223, 234, 255));
        }

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

        ImGui::SetCursorScreenPos(ImVec2(origin.x + 16.0f, origin.y + 16.0f));
        ImGui::TextUnformatted("Camera-driven viewport");
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 16.0f, origin.y + 34.0f));
        ImGui::TextDisabled("No offscreen image yet, but View/Projection is active");

        ImGui::SetCursorScreenPos(ImVec2(origin.x + 16.0f, origin.y + 58.0f));
        ImGui::Text("View matrix[0][0]: %.3f", view_[0][0]);
        ImGui::Text("Projection matrix[0][0]: %.3f", projection_[0][0]);
        ImGui::Text("VP matrix[0][0]: %.3f", view_projection_[0][0]);

        ImGui::Dummy(avail);
    }

    ImGui::End();
}

}  // namespace ui
