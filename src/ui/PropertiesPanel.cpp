#include "ui/PropertiesPanel.hpp"

#include <imgui.h>

namespace ui {

void PropertiesPanel::draw() {
    ImGui::Begin("Properties");
    ImGui::TextUnformatted("Command Bars");
    ImGui::Separator();
    ImGui::TextDisabled("No active selection");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Type: -");
        ImGui::BulletText("Name: -");
        ImGui::BulletText("Status: idle");
    }

    if (ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Shading: shaded");
        ImGui::BulletText("Projection: perspective");
        ImGui::BulletText("Camera: active");
    }

    if (ImGui::CollapsingHeader("AI", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Server: local");
        ImGui::BulletText("Policy: proposal-based");
    }

    if (ImGui::CollapsingHeader("History")) {
        ImGui::BulletText("Undo: 0");
        ImGui::BulletText("Redo: 0");
    }
    ImGui::End();
}

}  // namespace ui
