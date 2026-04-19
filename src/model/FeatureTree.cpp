#include "model/FeatureTree.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <unordered_set>

#include "geometry/GeometryThreadPool.hpp"

namespace model {

namespace {

const char* to_string(FeatureType type) {
    switch (type) {
    case FeatureType::PartContainer:
        return "PartContainer";
    case FeatureType::SketchFeature:
        return "SketchFeature";
    case FeatureType::ExtrudeFeature:
        return "ExtrudeFeature";
    case FeatureType::RevolveFeature:
        return "RevolveFeature";
    case FeatureType::FilletFeature:
        return "FilletFeature";
    case FeatureType::ChamferFeature:
        return "ChamferFeature";
    case FeatureType::ShellFeature:
        return "ShellFeature";
    case FeatureType::HoleFeature:
        return "HoleFeature";
    case FeatureType::MirrorFeature:
        return "MirrorFeature";
    }
    return "SketchFeature";
}

FeatureType feature_type_from_string(const std::string& value) {
    if (value == "PartContainer") {
        return FeatureType::PartContainer;
    }
    if (value == "SketchFeature") {
        return FeatureType::SketchFeature;
    }
    if (value == "ExtrudeFeature") {
        return FeatureType::ExtrudeFeature;
    }
    if (value == "RevolveFeature") {
        return FeatureType::RevolveFeature;
    }
    if (value == "FilletFeature") {
        return FeatureType::FilletFeature;
    }
    if (value == "ChamferFeature") {
        return FeatureType::ChamferFeature;
    }
    if (value == "ShellFeature") {
        return FeatureType::ShellFeature;
    }
    if (value == "HoleFeature") {
        return FeatureType::HoleFeature;
    }
    if (value == "MirrorFeature") {
        return FeatureType::MirrorFeature;
    }
    return FeatureType::SketchFeature;
}

const char* to_string(FeatureState state) {
    switch (state) {
    case FeatureState::Valid:
        return "Valid";
    case FeatureState::Warning:
        return "Warning";
    case FeatureState::Error:
        return "Error";
    case FeatureState::Suppressed:
        return "Suppressed";
    }
    return "Valid";
}

FeatureState feature_state_from_string(const std::string& value) {
    if (value == "Valid") {
        return FeatureState::Valid;
    }
    if (value == "Warning") {
        return FeatureState::Warning;
    }
    if (value == "Error") {
        return FeatureState::Error;
    }
    if (value == "Suppressed") {
        return FeatureState::Suppressed;
    }
    return FeatureState::Valid;
}

}  // namespace

FeatureTree::SnapshotCommand::SnapshotCommand(std::string before_snapshot, std::string after_snapshot)
    : before_snapshot_(std::move(before_snapshot)), after_snapshot_(std::move(after_snapshot)) {}

bool FeatureTree::SnapshotCommand::execute(FeatureTree& tree) {
    return tree.restore_json_snapshot(after_snapshot_);
}

bool FeatureTree::SnapshotCommand::undo(FeatureTree& tree) {
    return tree.restore_json_snapshot(before_snapshot_);
}

FeatureTree::FeatureTree() {
    root_ = allocate_node(FeatureType::PartContainer, "Part.001", nullptr);
    if (root_ != nullptr) {
        root_->state = FeatureState::Valid;
    }
}

uint32_t FeatureTree::create_feature(FeatureType type, const std::string& name, uint32_t parent_id, FeatureTreeError* error) {
    if (name.empty()) {
        if (error != nullptr) {
            *error = FeatureTreeError::InvalidName;
        }
        return 0U;
    }

    FeatureNode* parent = find_feature(parent_id);
    if (parent == nullptr) {
        if (error != nullptr) {
            *error = FeatureTreeError::InvalidParent;
        }
        return 0U;
    }

    const std::string before = to_json_snapshot();
    FeatureNode* created = allocate_node(type, name, parent);
    if (created == nullptr) {
        if (error != nullptr) {
            *error = FeatureTreeError::InvalidOperation;
        }
        return 0U;
    }

    const std::string after = to_json_snapshot();
    remember_snapshot(before, after);

    if (error != nullptr) {
        *error = FeatureTreeError::Ok;
    }

    return created->id;
}

FeatureTreeError FeatureTree::rename_feature(uint32_t feature_id, const std::string& name) {
    if (name.empty()) {
        return FeatureTreeError::InvalidName;
    }

    FeatureNode* node = find_feature(feature_id);
    if (node == nullptr) {
        return FeatureTreeError::FeatureNotFound;
    }

    const std::string before = to_json_snapshot();
    node->name = name;
    const std::string after = to_json_snapshot();
    remember_snapshot(before, after);

    return FeatureTreeError::Ok;
}

FeatureTreeError FeatureTree::set_suppressed(uint32_t feature_id, bool suppressed) {
    FeatureNode* node = find_feature(feature_id);
    if (node == nullptr) {
        return FeatureTreeError::FeatureNotFound;
    }

    const std::string before = to_json_snapshot();
    node->suppressed = suppressed;
    node->state = suppressed ? FeatureState::Suppressed : FeatureState::Valid;
    const std::string after = to_json_snapshot();
    remember_snapshot(before, after);

    return FeatureTreeError::Ok;
}

FeatureTreeError FeatureTree::delete_feature(uint32_t feature_id) {
    if (root_ == nullptr || feature_id == root_->id) {
        return FeatureTreeError::InvalidOperation;
    }

    FeatureNode* node = find_feature(feature_id);
    if (node == nullptr) {
        return FeatureTreeError::FeatureNotFound;
    }

    const std::string before = to_json_snapshot();

    if (node->parent != nullptr) {
        auto& siblings = node->parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), node), siblings.end());
    }

    erase_subtree(node);

    const std::string after = to_json_snapshot();
    remember_snapshot(before, after);

    return FeatureTreeError::Ok;
}

FeatureTreeError FeatureTree::reorder_feature(uint32_t dragged_id, uint32_t target_parent_id, size_t insert_index) {
    if (root_ == nullptr || dragged_id == root_->id) {
        return FeatureTreeError::InvalidOperation;
    }

    FeatureNode* dragged = find_feature(dragged_id);
    FeatureNode* target_parent = find_feature(target_parent_id);
    if (dragged == nullptr || target_parent == nullptr) {
        return FeatureTreeError::FeatureNotFound;
    }

    if (dragged == target_parent || is_descendant_of(target_parent, dragged)) {
        return FeatureTreeError::InvalidOperation;
    }

    const std::string before = to_json_snapshot();

    if (dragged->parent != nullptr) {
        auto& old_children = dragged->parent->children;
        old_children.erase(std::remove(old_children.begin(), old_children.end(), dragged), old_children.end());
    }

    auto& target_children = target_parent->children;
    const size_t safe_index = std::min(insert_index, target_children.size());
    target_children.insert(target_children.begin() + static_cast<std::ptrdiff_t>(safe_index), dragged);
    dragged->parent = target_parent;

    const std::string after = to_json_snapshot();
    remember_snapshot(before, after);

    return FeatureTreeError::Ok;
}

FeatureTreeError FeatureTree::set_feature_state(uint32_t feature_id, FeatureState state) {
    FeatureNode* node = find_feature(feature_id);
    if (node == nullptr) {
        return FeatureTreeError::FeatureNotFound;
    }

    node->state = state;
    node->suppressed = (state == FeatureState::Suppressed);
    return FeatureTreeError::Ok;
}

RebuildResult FeatureTree::rebuild(
    const RebuildRequest& request,
    const RebuildDelegate& delegate,
    const StageCallback& tessellate,
    const StageCallback& upload_mesh,
    const StageCallback& repaint) {
    RebuildResult result{};

    const FeatureNode* start = nullptr;
    if (request.full_rebuild) {
        start = root_;
    } else {
        start = find_feature(request.start_feature_id);
    }

    if (start == nullptr) {
        result.success = false;
        result.error = FeatureTreeError::FeatureNotFound;
        return result;
    }

    std::vector<uint32_t> order{};
    collect_topological_order(start, order);

    geometry::GeometryThreadPool pool(1U);

    for (const uint32_t node_id : order) {
        FeatureNode* node = find_feature(node_id);
        if (node == nullptr) {
            continue;
        }
        if (node->type == FeatureType::PartContainer) {
            continue;
        }
        if (node->suppressed || node->state == FeatureState::Suppressed) {
            continue;
        }

        std::mutex mutex{};
        std::condition_variable condition{};
        bool done = false;
        RebuildDelegateResult delegate_result{};

        const nlohmann::json payload = {
            {"command", "rebuild_feature"},
            {"feature_id", node->id},
            {"feature_name", node->name},
            {"feature_type", to_string(node->type)},
            {"full_rebuild", request.full_rebuild},
            {"start_feature_id", request.start_feature_id},
        };

        const bool queued = pool.enqueue([&]() {
            if (delegate) {
                delegate_result = delegate(payload);
            }

            {
                std::scoped_lock lock(mutex);
                done = true;
            }
            condition.notify_one();
        });

        if (!queued) {
            result.success = false;
            result.error = FeatureTreeError::QueueFailed;
            node->state = FeatureState::Error;
            result.failed_feature_id = node->id;
            return result;
        }

        {
            std::unique_lock lock(mutex);
            condition.wait(lock, [&]() { return done; });
        }

        if (delegate_result.worker_crashed) {
            node->state = FeatureState::Warning;
            result.success = false;
            result.worker_crashed = true;
            result.failed_feature_id = node->id;
            result.error = FeatureTreeError::WorkerCrashed;
            return result;
        }

        if (!delegate_result.success) {
            node->state = FeatureState::Error;
            result.success = false;
            result.failed_feature_id = node->id;
            result.error = FeatureTreeError::WorkerFailed;
            return result;
        }

        node->state = FeatureState::Valid;
    }

    if (tessellate) {
        tessellate();
    }
    if (upload_mesh) {
        upload_mesh();
    }
    if (repaint) {
        repaint();
    }

    result.success = true;
    result.error = FeatureTreeError::Ok;
    return result;
}

FeatureNode* FeatureTree::root() {
    return root_;
}

const FeatureNode* FeatureTree::root() const {
    return root_;
}

FeatureNode* FeatureTree::find_feature(uint32_t feature_id) {
    const auto it = node_index_.find(feature_id);
    if (it == node_index_.end()) {
        return nullptr;
    }
    return it->second;
}

const FeatureNode* FeatureTree::find_feature(uint32_t feature_id) const {
    const auto it = node_index_.find(feature_id);
    if (it == node_index_.end()) {
        return nullptr;
    }
    return it->second;
}

size_t FeatureTree::node_count() const {
    return node_index_.size();
}

bool FeatureTree::can_undo() const {
    return !undo_stack_.empty();
}

bool FeatureTree::can_redo() const {
    return !redo_stack_.empty();
}

bool FeatureTree::undo() {
    if (undo_stack_.empty()) {
        return false;
    }

    std::unique_ptr<FeatureCommand> command = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    if (!command->undo(*this)) {
        return false;
    }

    redo_stack_.push_back(std::move(command));
    return true;
}

bool FeatureTree::redo() {
    if (redo_stack_.empty()) {
        return false;
    }

    std::unique_ptr<FeatureCommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!command->execute(*this)) {
        return false;
    }

    undo_stack_.push_back(std::move(command));
    return true;
}

std::string FeatureTree::to_json_snapshot() const {
    nlohmann::json root{};
    root["next_id"] = next_id_;
    root["tree_root"] = serialize_node(root_);
    return root.dump();
}

bool FeatureTree::restore_json_snapshot(const std::string& snapshot) {
    const nlohmann::json parsed = nlohmann::json::parse(snapshot, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return false;
    }
    if (!parsed.contains("next_id") || !parsed.contains("tree_root")) {
        return false;
    }

    const bool old_history = history_enabled_;
    history_enabled_ = false;

    storage_.clear();
    node_index_.clear();
    root_ = nullptr;

    next_id_ = parsed["next_id"].get<uint32_t>();
    if (!deserialize_node(parsed["tree_root"], nullptr)) {
        storage_.clear();
        node_index_.clear();
        root_ = nullptr;
        history_enabled_ = old_history;
        return false;
    }

    history_enabled_ = old_history;
    return true;
}

FeatureNode* FeatureTree::allocate_node(FeatureType type, const std::string& name, FeatureNode* parent, uint32_t forced_id) {
    const uint32_t id = forced_id == 0U ? next_id_++ : forced_id;
    if (forced_id != 0U) {
        next_id_ = std::max(next_id_, forced_id + 1U);
    }

    auto node = std::make_unique<FeatureNode>();
    node->id = id;
    node->type = type;
    node->name = name;
    node->state = FeatureState::Valid;
    node->parent = parent;

    FeatureNode* raw = node.get();
    storage_.push_back(std::move(node));
    node_index_[id] = raw;

    if (parent != nullptr) {
        parent->children.push_back(raw);
    }

    return raw;
}

void FeatureTree::erase_subtree(FeatureNode* node) {
    if (node == nullptr) {
        return;
    }

    std::vector<FeatureNode*> stack{node};
    std::unordered_set<uint32_t> ids{};
    while (!stack.empty()) {
        FeatureNode* current = stack.back();
        stack.pop_back();
        ids.insert(current->id);
        for (FeatureNode* child : current->children) {
            stack.push_back(child);
        }
    }

    storage_.erase(std::remove_if(storage_.begin(), storage_.end(), [&](const std::unique_ptr<FeatureNode>& owned) {
        return ids.contains(owned->id);
    }), storage_.end());

    for (uint32_t id : ids) {
        node_index_.erase(id);
    }
}

bool FeatureTree::is_descendant_of(const FeatureNode* candidate, const FeatureNode* ancestor) const {
    const FeatureNode* cursor = candidate;
    while (cursor != nullptr) {
        if (cursor == ancestor) {
            return true;
        }
        cursor = cursor->parent;
    }
    return false;
}

void FeatureTree::collect_topological_order(const FeatureNode* start, std::vector<uint32_t>& out_order) const {
    if (start == nullptr) {
        return;
    }

    out_order.push_back(start->id);
    for (const FeatureNode* child : start->children) {
        collect_topological_order(child, out_order);
    }
}

nlohmann::json FeatureTree::serialize_node(const FeatureNode* node) const {
    if (node == nullptr) {
        return {};
    }

    nlohmann::json children = nlohmann::json::array();
    for (const FeatureNode* child : node->children) {
        children.push_back(serialize_node(child));
    }

    return {
        {"id", node->id},
        {"type", to_string(node->type)},
        {"name", node->name},
        {"state", to_string(node->state)},
        {"suppressed", node->suppressed},
        {"children", children},
    };
}

bool FeatureTree::deserialize_node(const nlohmann::json& data, FeatureNode* parent) {
    if (!data.is_object()) {
        return false;
    }

    if (!data.contains("id") || !data.contains("type") || !data.contains("name") || !data.contains("children")) {
        return false;
    }

    const uint32_t id = data["id"].get<uint32_t>();
    const FeatureType type = feature_type_from_string(data["type"].get<std::string>());
    const std::string name = data["name"].get<std::string>();

    FeatureNode* node = allocate_node(type, name, parent, id);
    if (node == nullptr) {
        return false;
    }

    if (parent == nullptr) {
        root_ = node;
    }

    if (data.contains("state")) {
        node->state = feature_state_from_string(data["state"].get<std::string>());
    }
    if (data.contains("suppressed")) {
        node->suppressed = data["suppressed"].get<bool>();
    }

    for (const auto& child : data["children"]) {
        if (!deserialize_node(child, node)) {
            return false;
        }
    }

    return true;
}

void FeatureTree::remember_snapshot(const std::string& before_snapshot, const std::string& after_snapshot) {
    if (!history_enabled_) {
        return;
    }

    redo_stack_.clear();
    undo_stack_.push_back(std::make_unique<SnapshotCommand>(before_snapshot, after_snapshot));
    while (undo_stack_.size() > k_history_limit) {
        undo_stack_.pop_front();
    }
}

}  // namespace model
