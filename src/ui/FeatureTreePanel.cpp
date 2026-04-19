#include "ui/FeatureTreePanel.hpp"

#include <algorithm>

#include <imgui.h>

namespace ui {

void FeatureTreePanel::set_font_scale(float scale) {
    font_scale_ = std::max(scale, 0.7f);
}

bool FeatureTreePanel::consume_open_sketch_request() {
    const bool requested = open_sketch_request_;
    open_sketch_request_ = false;
    return requested;
}

bool FeatureTreePanel::consume_open_plane_properties_request() {
    const bool requested = open_plane_properties_request_;
    open_plane_properties_request_ = false;
    return requested;
}

void FeatureTreePanel::draw() {
    ImGui::Begin("FeatureTree");
    ImGui::SetWindowFontScale(font_scale_);
    ImGui::TextUnformatted("Specification Tree");
    ImGui::Separator();

    if (ImGui::TreeNode("Part.001")) {
        constexpr ImGuiTreeNodeFlags sketch_flags = ImGuiTreeNodeFlags_SpanFullWidth;
        const bool sketch_open = ImGui::TreeNodeEx("Sketch.001", sketch_flags);
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            open_sketch_request_ = true;
        }

        if (sketch_open) {
            constexpr ImGuiSelectableFlags plane_flags = ImGuiSelectableFlags_AllowDoubleClick;
            if (ImGui::Selectable("Plane.001", false, plane_flags) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                open_plane_properties_request_ = true;
                open_sketch_request_ = false;
            }
            ImGui::TreePop();
        }

        ImGui::BulletText("Pad.001");
        ImGui::BulletText("Fillet.001");
        ImGui::BulletText("Shell.001");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Reference Geometry")) {
        ImGui::BulletText("Origin");
        ImGui::BulletText("XY Plane");
        ImGui::BulletText("XZ Plane");
        ImGui::BulletText("YZ Plane");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Constraints")) {
        ImGui::TextDisabled("No active constraints");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Parameters")) {
        ImGui::BulletText("Length.001 = 120 mm");
        ImGui::BulletText("Radius.001 = 12 mm");
        ImGui::BulletText("Angle.001 = 45 deg");
        ImGui::TreePop();
    }

    ImGui::End();
}

}  // namespace ui
