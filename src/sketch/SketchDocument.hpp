#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "geometry/IGeometryKernel.hpp"
#include "sketch/SketchConstraint.hpp"
#include "sketch/SketchEntity.hpp"
#include "sketch/Plane.hpp"
#include "sketch/SketchSolver.hpp"

namespace sketch {

/**
 * @brief Mutable sketch model containing entities and constraints.
 */
class SketchDocument {
public:
    /**
     * @brief Creates sketch document on a plane defined by feature point.
     * @param feature_point Feature point used as plane origin.
     */
    explicit SketchDocument(const glm::vec3& feature_point);

    /**
     * @brief Enters sketch editing mode.
     */
    void enter();

    /**
     * @brief Exits sketch editing mode.
     */
    void exit();

    /**
     * @brief Returns whether sketch mode is active.
     * @return True when active.
     */
    [[nodiscard]] bool is_active() const;

    /**
     * @brief Creates a point entity.
     * @param pos Position in sketch plane (mm).
     * @param construction Construction flag.
     * @return Created entity id.
     */
    entity_id add_point(const glm::vec2& pos, bool construction = false);

    /**
     * @brief Creates a line entity.
     * @param p1 Start point.
     * @param p2 End point.
     * @param construction Construction flag.
     * @return Created entity id.
     */
    entity_id add_line(const glm::vec2& p1, const glm::vec2& p2, bool construction = false);

    /**
     * @brief Creates a circle entity.
     * @param center Circle center.
     * @param radius Circle radius.
     * @param construction Construction flag.
     * @return Created entity id.
     */
    entity_id add_circle(const glm::vec2& center, float radius, bool construction = false);

    /**
     * @brief Creates an arc entity.
     * @param center Arc center.
     * @param radius Arc radius.
     * @param angle_start Start angle in degrees.
     * @param angle_end End angle in degrees.
     * @param construction Construction flag.
     * @return Created entity id.
     */
    entity_id add_arc(const glm::vec2& center, float radius, float angle_start, float angle_end, bool construction = false);

    /**
     * @brief Adds a geometric or dimensional constraint.
     * @param data Constraint payload.
     * @return Created constraint id.
     */
    constraint_id add_constraint(const SketchConstraintData& data);

    /**
     * @brief Solves all active constraints.
     * @return Last solve result.
     */
    SolveResult solve();

    /**
     * @brief Extracts closed profiles from solved entities.
     * @return Closed profile list.
     */
    std::vector<geometry::Profile> extract_profiles() const;

    /**
     * @brief Extracts preview curves for viewport display, including open entities.
     * @return Curve list as profiles (open and closed).
     */
    std::vector<geometry::Profile> extract_preview_profiles() const;

    /**
     * @brief Returns constrained entity id set.
     * @return Entity ids used by any constraint.
     */
    std::unordered_set<entity_id> constrained_entities() const;

    /**
     * @brief Finds entity by id.
     * @param id Entity id.
     * @return Pointer to mutable entity.
     */
    SketchEntity* find_entity(entity_id id);

    /**
     * @brief Finds entity by id.
     * @param id Entity id.
     * @return Pointer to entity.
     */
    const SketchEntity* find_entity(entity_id id) const;

    /**
     * @brief Access to entity list.
     * @return Entity array.
     */
    [[nodiscard]] const std::vector<SketchEntity>& entities() const;

    /**
     * @brief Access to constraint list.
     * @return Constraint array.
     */
    [[nodiscard]] const std::vector<SketchConstraint>& constraints() const;

    /**
     * @brief Returns current DOF estimate.
     * @return DOF value.
     */
    [[nodiscard]] int dof() const;

    /**
     * @brief Returns last solve result.
     * @return Solve result.
     */
    [[nodiscard]] const SolveResult& last_solve_result() const;

    /**
     * @brief Returns active sketch plane.
     * @return Plane definition.
     */
    [[nodiscard]] const Plane& plane() const;

    /**
     * @brief Sets active sketch plane.
     * @param plane New plane definition.
     */
    void set_plane(const Plane& plane);

    /**
     * @brief Adds grid feature on current plane.
     * @return Grid feature id.
     */
    uint32_t add_grid_feature_on_plane();

    /**
     * @brief Returns whether active grid feature exists.
     * @return True when grid feature exists.
     */
    [[nodiscard]] bool has_grid_feature() const;

    /**
     * @brief Returns active grid feature.
     * @return Pointer to grid feature or nullptr.
     */
    [[nodiscard]] const GridFeature* active_grid_feature() const;

    /**
     * @brief Returns active grid feature.
     * @return Pointer to grid feature or nullptr.
     */
    GridFeature* active_grid_feature();

    /**
     * @brief Returns sketch snap state.
     * @return True when snap is enabled.
     */
    [[nodiscard]] bool snap_enabled() const;

    /**
     * @brief Sets sketch snap state.
     * @param enabled Snap state.
     */
    void set_snap_enabled(bool enabled);

private:
    std::vector<SketchEntity> entities_{};
    std::vector<SketchConstraint> constraints_{};
    SketchSolver solver_{};
    SolveResult last_result_{};
    Plane plane_;
    std::vector<GridFeature> grid_features_{};
    uint32_t next_grid_feature_id_ = 1U;
    entity_id next_entity_id_ = 1U;
    constraint_id next_constraint_id_ = 1U;
    bool snap_enabled_ = true;
    bool active_ = false;
};

}  // namespace sketch
