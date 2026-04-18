#include "ui/FeatureTreePanel.hpp"

#include <imgui.h>

namespace ui {

void FeatureTreePanel::draw() {
    ImGui::Begin("FeatureTree");
    ImGui::TextUnformatted("Specification Tree");
    ImGui::Separator();

    if (ImGui::TreeNode("Part.001")) {
        ImGui::BulletText("Sketch.001");
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
