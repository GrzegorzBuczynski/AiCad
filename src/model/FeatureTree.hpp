#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "model/FeatureNode.hpp"

namespace model {

/**
 * @brief Error codes used by feature tree mutation APIs.
 */
enum class FeatureTreeError {
    Ok,
    FeatureNotFound,
    InvalidParent,
    InvalidOperation,
    InvalidName,
    QueueFailed,
    WorkerFailed,
    WorkerCrashed,
};

/**
 * @brief Single worker delegation result used by rebuild pipeline.
 */
struct RebuildDelegateResult {
    bool success = true;
    bool worker_crashed = false;
    std::string message{};
};

/**
 * @brief Rebuild request descriptor consumed by orchestrator.
 */
struct RebuildRequest {
    bool full_rebuild = true;
    uint32_t start_feature_id = 0U;
};

/**
 * @brief Result of rebuild pipeline execution.
 */
struct RebuildResult {
    bool success = true;
    bool worker_crashed = false;
    uint32_t failed_feature_id = 0U;
    FeatureTreeError error = FeatureTreeError::Ok;
};

/**
 * @brief Parametric feature tree model with rebuild and history support.
 */
class FeatureTree {
public:
    using RebuildDelegate = std::function<RebuildDelegateResult(const nlohmann::json& payload)>;
    using StageCallback = std::function<void()>;

    /**
     * @brief Constructs default tree with root part container.
     */
    FeatureTree();

    /**
     * @brief Creates feature under selected parent feature.
     * @param type Feature type.
     * @param name User-visible feature name.
     * @param parent_id Parent feature id.
     * @param error Optional output error code.
     * @return Created feature id or 0 on failure.
     */
    uint32_t create_feature(FeatureType type, const std::string& name, uint32_t parent_id, FeatureTreeError* error = nullptr);

    /**
     * @brief Renames an existing feature.
    * @param feature_id Target feature id.
     * @param name New feature name.
     * @return Operation status.
     */
    FeatureTreeError rename_feature(uint32_t feature_id, const std::string& name);

    /**
     * @brief Sets suppression state for selected feature.
    * @param feature_id Target feature id.
     * @param suppressed Suppression flag.
     * @return Operation status.
     */
    FeatureTreeError set_suppressed(uint32_t feature_id, bool suppressed);

    /**
     * @brief Deletes feature subtree.
    * @param feature_id Feature id to delete.
     * @return Operation status.
     */
    FeatureTreeError delete_feature(uint32_t feature_id);

    /**
     * @brief Reorders dragged feature into target parent at selected index.
     * @param dragged_id Dragged feature id.
     * @param target_parent_id New parent feature id.
     * @param insert_index Child insertion index.
     * @return Operation status.
     */
    FeatureTreeError reorder_feature(uint32_t dragged_id, uint32_t target_parent_id, size_t insert_index);

    /**
     * @brief Sets explicit feature state.
    * @param feature_id Target feature id.
     * @param state New state value.
     * @return Operation status.
     */
    FeatureTreeError set_feature_state(uint32_t feature_id, FeatureState state);

    /**
     * @brief Executes full or partial rebuild in topological order.
     * @param request Full or partial rebuild descriptor.
     * @param delegate Worker-side build callback.
     * @param tessellate Tessellation stage callback.
     * @param upload_mesh GPU upload callback.
     * @param repaint UI repaint callback.
     * @return Rebuild result details.
     */
    RebuildResult rebuild(
        const RebuildRequest& request,
        const RebuildDelegate& delegate,
        const StageCallback& tessellate,
        const StageCallback& upload_mesh,
        const StageCallback& repaint);

    /**
     * @brief Returns root feature.
     * @return Pointer to root feature.
     */
    [[nodiscard]] FeatureNode* root();

    /**
     * @brief Returns root feature.
     * @return Pointer to root feature.
     */
    [[nodiscard]] const FeatureNode* root() const;

    /**
     * @brief Finds feature by id.
    * @param feature_id Feature id.
     * @return Pointer to feature or nullptr.
     */
    [[nodiscard]] FeatureNode* find_feature(uint32_t feature_id);

    /**
     * @brief Finds feature by id.
    * @param feature_id Feature id.
     * @return Pointer to feature or nullptr.
     */
    [[nodiscard]] const FeatureNode* find_feature(uint32_t feature_id) const;

    /**
     * @brief Returns total number of features.
     * @return Feature count.
     */
    [[nodiscard]] size_t node_count() const;

    /**
     * @brief Returns whether undo stack is non-empty.
     * @return True when undo can be executed.
     */
    [[nodiscard]] bool can_undo() const;

    /**
     * @brief Returns whether redo stack is non-empty.
     * @return True when redo can be executed.
     */
    [[nodiscard]] bool can_redo() const;

    /**
     * @brief Executes undo command.
     * @return True when operation succeeded.
     */
    bool undo();

    /**
     * @brief Executes redo command.
     * @return True when operation succeeded.
     */
    bool redo();

    /**
     * @brief Serializes tree into JSON snapshot string.
     * @return Serialized snapshot.
     */
    [[nodiscard]] std::string to_json_snapshot() const;

    /**
     * @brief Restores tree from snapshot string.
     * @param snapshot JSON snapshot string.
     * @return True when snapshot was restored.
     */
    bool restore_json_snapshot(const std::string& snapshot);

private:
    class FeatureCommand {
    public:
        virtual ~FeatureCommand() = default;
        virtual bool execute(FeatureTree& tree) = 0;
        virtual bool undo(FeatureTree& tree) = 0;
    };

    class SnapshotCommand final : public FeatureCommand {
    public:
        SnapshotCommand(std::string before_snapshot, std::string after_snapshot);
        bool execute(FeatureTree& tree) override;
        bool undo(FeatureTree& tree) override;

    private:
        std::string before_snapshot_{};
        std::string after_snapshot_{};
    };

    FeatureNode* allocate_node(FeatureType type, const std::string& name, FeatureNode* parent, uint32_t forced_id = 0U);
    void erase_subtree(FeatureNode* node);
    bool is_descendant_of(const FeatureNode* candidate, const FeatureNode* ancestor) const;
    void collect_topological_order(const FeatureNode* start, std::vector<uint32_t>& out_order) const;
    nlohmann::json serialize_node(const FeatureNode* node) const;
    bool deserialize_node(const nlohmann::json& data, FeatureNode* parent);
    void remember_snapshot(const std::string& before_snapshot, const std::string& after_snapshot);

    uint32_t next_id_ = 1U;
    FeatureNode* root_ = nullptr;
    std::vector<std::unique_ptr<FeatureNode>> storage_{};
    std::unordered_map<uint32_t, FeatureNode*> node_index_{};

    std::deque<std::unique_ptr<FeatureCommand>> undo_stack_{};
    std::deque<std::unique_ptr<FeatureCommand>> redo_stack_{};
    bool history_enabled_ = true;
    static constexpr size_t k_history_limit = 50U;
};

}  // namespace model
