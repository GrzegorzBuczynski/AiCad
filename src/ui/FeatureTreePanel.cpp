#include "ui/FeatureTreePanel.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>

#include <imgui.h>

namespace ui {

void FeatureTreePanel::set_feature_tree(model::FeatureTree* tree) {
    tree_ = tree;
}

void FeatureTreePanel::set_font_scale(float scale) {
    font_scale_ = std::max(scale, 0.7f);
}

std::optional<RebuildIntent> FeatureTreePanel::consume_rebuild_intent() {
    std::optional<RebuildIntent> intent = rebuild_intent_;
    rebuild_intent_.reset();
    return intent;
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

const char* FeatureTreePanel::icon_for_type(model::FeatureType type) {
    switch (type) {
    case model::FeatureType::PartContainer:
        return "[PRT]";
    case model::FeatureType::SketchFeature:
        return "[SK]";
    case model::FeatureType::ExtrudeFeature:
        return "[EX]";
    case model::FeatureType::RevolveFeature:
        return "[RV]";
    case model::FeatureType::FilletFeature:
        return "[FL]";
    case model::FeatureType::ChamferFeature:
        return "[CH]";
    case model::FeatureType::ShellFeature:
        return "[SH]";
    case model::FeatureType::HoleFeature:
        return "[HO]";
    case model::FeatureType::MirrorFeature:
        return "[MR]";
    }

    return "[??]";
}

uint32_t FeatureTreePanel::color_for_state(model::FeatureState state) {
    switch (state) {
    case model::FeatureState::Valid:
        return IM_COL32(32, 52, 84, 255);
    case model::FeatureState::Warning:
        return IM_COL32(201, 120, 36, 255);
    case model::FeatureState::Error:
        return IM_COL32(184, 52, 52, 255);
    case model::FeatureState::Suppressed:
        return IM_COL32(122, 128, 138, 255);
    }

    return IM_COL32(32, 52, 84, 255);
}

void FeatureTreePanel::draw_node(model::FeatureNode* node) {
    if (tree_ == nullptr || node == nullptr) {
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
    if (node->children.empty() && node->type != model::FeatureType::SketchFeature) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    const std::string label = std::string(icon_for_type(node->type)) + " " + node->name + "##node_" + std::to_string(node->id);
    ImGui::PushStyleColor(ImGuiCol_Text, color_for_state(node->state));
    const bool node_open = ImGui::TreeNodeEx(label.c_str(), flags);
    ImGui::PopStyleColor();

    if (node->type == model::FeatureType::SketchFeature && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        open_sketch_request_ = true;
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Edit")) {
            rebuild_intent_ = RebuildIntent{false, node->id};
        }

        if (ImGui::MenuItem("Rename")) {
            rename_node_id_ = node->id;
            std::snprintf(rename_buffer_.data(), rename_buffer_.size(), "%s", node->name.c_str());
            ImGui::OpenPopup("Rename Feature");
        }

        const bool suppressed = node->state == model::FeatureState::Suppressed || node->suppressed;
        if (!suppressed) {
            if (ImGui::MenuItem("Suppress")) {
                if (tree_->set_suppressed(node->id, true) == model::FeatureTreeError::Ok) {
                    rebuild_intent_ = RebuildIntent{false, node->id};
                }
            }
        } else {
            if (ImGui::MenuItem("Unsuppress")) {
                if (tree_->set_suppressed(node->id, false) == model::FeatureTreeError::Ok) {
                    rebuild_intent_ = RebuildIntent{false, node->id};
                }
            }
        }

        if (node->parent != nullptr && ImGui::MenuItem("Delete")) {
            const uint32_t rebuild_start = node->parent->id;
            if (tree_->delete_feature(node->id) == model::FeatureTreeError::Ok) {
                rebuild_intent_ = RebuildIntent{false, rebuild_start};
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Reorder by drag and drop");
        ImGui::EndPopup();
    }

    if (ImGui::BeginDragDropSource()) {
        const uint32_t drag_id = node->id;
        ImGui::SetDragDropPayload("VULCANCAD_FEATURE_NODE", &drag_id, sizeof(drag_id));
        ImGui::Text("Move %s", node->name.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VULCANCAD_FEATURE_NODE")) {
            const uint32_t dragged_id = *static_cast<const uint32_t*>(payload->Data);
            if (dragged_id != node->id && node->parent != nullptr) {
                auto& siblings = node->parent->children;
                const auto it = std::find(siblings.begin(), siblings.end(), node);
                if (it != siblings.end()) {
                    const size_t insert_index = static_cast<size_t>(std::distance(siblings.begin(), it)) + 1U;
                    if (tree_->reorder_feature(dragged_id, node->parent->id, insert_index) == model::FeatureTreeError::Ok) {
                        rebuild_intent_ = RebuildIntent{false, node->parent->id};
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (node_open) {
        if (node->type == model::FeatureType::SketchFeature) {
            constexpr ImGuiSelectableFlags plane_flags = ImGuiSelectableFlags_AllowDoubleClick;
            if (ImGui::Selectable("Plane.001", false, plane_flags) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                open_plane_properties_request_ = true;
                open_sketch_request_ = false;
            }
        }

        for (model::FeatureNode* child : node->children) {
            draw_node(child);
        }
        ImGui::TreePop();
    }
}

void FeatureTreePanel::draw() {
    ImGui::Begin("FeatureTree");
    ImGui::SetWindowFontScale(font_scale_);
    ImGui::TextUnformatted("Specification Tree");
    if (tree_ != nullptr) {
        ImGui::SameLine();
        if (ImGui::Button("Full Rebuild")) {
            const model::FeatureNode* tree_root = tree_->root();
            rebuild_intent_ = RebuildIntent{true, tree_root != nullptr ? tree_root->id : 0U};
        }

        ImGui::SameLine();
        const bool can_undo = tree_->can_undo();
        ImGui::BeginDisabled(!can_undo);
        if (ImGui::Button("Undo")) {
            if (tree_->undo()) {
                const model::FeatureNode* tree_root = tree_->root();
                rebuild_intent_ = RebuildIntent{true, tree_root != nullptr ? tree_root->id : 0U};
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        const bool can_redo = tree_->can_redo();
        ImGui::BeginDisabled(!can_redo);
        if (ImGui::Button("Redo")) {
            if (tree_->redo()) {
                const model::FeatureNode* tree_root = tree_->root();
                rebuild_intent_ = RebuildIntent{true, tree_root != nullptr ? tree_root->id : 0U};
            }
        }
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    if (tree_ == nullptr || tree_->root() == nullptr) {
        ImGui::TextDisabled("Feature tree not initialized");
    } else {
        draw_node(tree_->root());
    }

    if (rename_node_id_ != 0U) {
        ImGui::SetNextWindowSize(ImVec2(360.0f, 120.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Rename Feature", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::InputText("Name", rename_buffer_.data(), rename_buffer_.size());
            if (ImGui::Button("Apply")) {
                if (tree_ != nullptr && tree_->rename_feature(rename_node_id_, std::string(rename_buffer_.data())) == model::FeatureTreeError::Ok) {
                    rebuild_intent_ = RebuildIntent{false, rename_node_id_};
                }
                rename_node_id_ = 0U;
                rename_buffer_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                rename_node_id_ = 0U;
                rename_buffer_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else {
            ImGui::OpenPopup("Rename Feature");
        }
    }

    ImGui::End();
}

}  // namespace ui
