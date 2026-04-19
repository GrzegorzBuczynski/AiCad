#include "ui/FeatureTreePanel.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>

#include <imgui.h>

#include "sketch/SketchDocument.hpp"
#include "sketch/SketchEntity.hpp"

namespace ui {

void FeatureTreePanel::set_feature_tree(model::FeatureTree* tree) {
    tree_ = tree;
}

void FeatureTreePanel::set_sketch_document(sketch::SketchDocument* document) {
    sketch_document_ = document;
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

void FeatureTreePanel::draw_sketch_entity_hierarchy() {
    if (sketch_document_ == nullptr) {
        ImGui::TextDisabled("No sketch document attached");
        return;
    }

    const ImGuiTreeNodeFlags collector_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth;
    if (!ImGui::TreeNodeEx("[GEO] Geometry", collector_flags)) {
        return;
    }

    const std::vector<sketch::SketchEntity>& entities = sketch_document_->entities();
    for (const sketch::SketchEntity& entity : entities) {
        const auto* line = std::get_if<sketch::LineEntity>(&entity.data);
        if (line == nullptr) {
            continue;
        }

        const std::string line_label = "[LN] Line." + std::to_string(entity.id) + "##sk_line_" + std::to_string(entity.id);
        const bool line_open = ImGui::TreeNodeEx(line_label.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
        if (!line_open) {
            continue;
        }

        const auto point_ids = sketch_document_->line_points(entity.id);
        if (!point_ids.has_value()) {
            ImGui::TextDisabled("Missing endpoint references");
            ImGui::TreePop();
            continue;
        }

        const sketch::SketchEntity* point_a = sketch_document_->find_entity(point_ids->first);
        const sketch::SketchEntity* point_b = sketch_document_->find_entity(point_ids->second);

        const auto draw_point = [this](const sketch::SketchEntity* point_entity, const char* slot_label) {
            if (point_entity == nullptr) {
                ImGui::BulletText("[PT] %s: missing", slot_label);
                return;
            }

            const auto* point = std::get_if<sketch::PointEntity>(&point_entity->data);
            if (point == nullptr) {
                ImGui::BulletText("[PT] %s: invalid", slot_label);
                return;
            }

            const std::string point_label =
                std::string("[PT] ") + slot_label +
                " Point." + std::to_string(point_entity->id) +
                " (" + std::to_string(point->pos.x) + ", " + std::to_string(point->pos.y) + ")##sk_pt_" +
                std::to_string(point_entity->id) + "_" + slot_label;

            constexpr ImGuiSelectableFlags point_flags = ImGuiSelectableFlags_AllowDoubleClick;
            if (ImGui::Selectable(point_label.c_str(), false, point_flags) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                edit_point_id_ = point_entity->id;
                edit_point_value_ = point->pos;
                ImGui::OpenPopup("Edit Point Coordinates");
            }
        };

        draw_point(point_a, "A");
        draw_point(point_b, "B");

        ImGui::TreePop();
    }

    ImGui::TreePop();
}

void FeatureTreePanel::draw_feature_node(model::FeatureNode* feature) {
    if (tree_ == nullptr || feature == nullptr) {
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
    if (feature->children.empty() && feature->type != model::FeatureType::SketchFeature) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    const std::string label = std::string(icon_for_type(feature->type)) + " " + feature->name + "##feature_" + std::to_string(feature->id);
    ImGui::PushStyleColor(ImGuiCol_Text, color_for_state(feature->state));
    const bool feature_open = ImGui::TreeNodeEx(label.c_str(), flags);
    ImGui::PopStyleColor();

    if (feature->type == model::FeatureType::SketchFeature && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        open_sketch_request_ = true;
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Edit")) {
            rebuild_intent_ = RebuildIntent{false, feature->id};
        }

        if (ImGui::MenuItem("Rename")) {
            rename_feature_id_ = feature->id;
            std::snprintf(rename_buffer_.data(), rename_buffer_.size(), "%s", feature->name.c_str());
            ImGui::OpenPopup("Rename Feature");
        }

        const bool suppressed = feature->state == model::FeatureState::Suppressed || feature->suppressed;
        if (!suppressed) {
            if (ImGui::MenuItem("Suppress")) {
                if (tree_->set_suppressed(feature->id, true) == model::FeatureTreeError::Ok) {
                    rebuild_intent_ = RebuildIntent{false, feature->id};
                }
            }
        } else {
            if (ImGui::MenuItem("Unsuppress")) {
                if (tree_->set_suppressed(feature->id, false) == model::FeatureTreeError::Ok) {
                    rebuild_intent_ = RebuildIntent{false, feature->id};
                }
            }
        }

        if (feature->parent != nullptr && ImGui::MenuItem("Delete")) {
            const uint32_t rebuild_start = feature->parent->id;
            if (tree_->delete_feature(feature->id) == model::FeatureTreeError::Ok) {
                rebuild_intent_ = RebuildIntent{false, rebuild_start};
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Reorder by drag and drop");
        ImGui::EndPopup();
    }

    if (ImGui::BeginDragDropSource()) {
        const uint32_t drag_id = feature->id;
        ImGui::SetDragDropPayload("VULCANCAD_FEATURE_NODE", &drag_id, sizeof(drag_id));
        ImGui::Text("Move %s", feature->name.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VULCANCAD_FEATURE_NODE")) {
            const uint32_t dragged_id = *static_cast<const uint32_t*>(payload->Data);
            if (dragged_id != feature->id && feature->parent != nullptr) {
                auto& siblings = feature->parent->children;
                const auto it = std::find(siblings.begin(), siblings.end(), feature);
                if (it != siblings.end()) {
                    const size_t insert_index = static_cast<size_t>(std::distance(siblings.begin(), it)) + 1U;
                    if (tree_->reorder_feature(dragged_id, feature->parent->id, insert_index) == model::FeatureTreeError::Ok) {
                        rebuild_intent_ = RebuildIntent{false, feature->parent->id};
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (feature_open) {
        if (feature->type == model::FeatureType::SketchFeature) {
            constexpr ImGuiSelectableFlags plane_flags = ImGuiSelectableFlags_AllowDoubleClick;
            if (ImGui::Selectable("Plane.001", false, plane_flags) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                open_plane_properties_request_ = true;
                open_sketch_request_ = false;
            }

            draw_sketch_entity_hierarchy();
        }

        for (model::FeatureNode* child : feature->children) {
            draw_feature_node(child);
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
        draw_feature_node(tree_->root());
    }

    if (rename_feature_id_ != 0U) {
        ImGui::SetNextWindowSize(ImVec2(360.0f, 120.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Rename Feature", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::InputText("Name", rename_buffer_.data(), rename_buffer_.size());
            if (ImGui::Button("Apply")) {
                if (tree_ != nullptr && tree_->rename_feature(rename_feature_id_, std::string(rename_buffer_.data())) == model::FeatureTreeError::Ok) {
                    rebuild_intent_ = RebuildIntent{false, rename_feature_id_};
                }
                rename_feature_id_ = 0U;
                rename_buffer_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                rename_feature_id_ = 0U;
                rename_buffer_.fill('\0');
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else {
            ImGui::OpenPopup("Rename Feature");
        }
    }

    if (edit_point_id_ != 0U) {
        ImGui::SetNextWindowSize(ImVec2(380.0f, 140.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Edit Point Coordinates", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::DragFloat2("XY (mm)", &edit_point_value_.x, 0.1f, -100000.0f, 100000.0f, "%.3f");
            if (ImGui::Button("Apply")) {
                if (sketch_document_ != nullptr) {
                    sketch::SketchEntity* point_entity = sketch_document_->find_entity(edit_point_id_);
                    if (point_entity != nullptr) {
                        (void)sketch::set_point(point_entity, 0U, edit_point_value_);
                        sketch_document_->solve();
                    }
                }

                edit_point_id_ = 0U;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                edit_point_id_ = 0U;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else {
            ImGui::OpenPopup("Edit Point Coordinates");
        }
    }

    ImGui::End();
}

}  // namespace ui
