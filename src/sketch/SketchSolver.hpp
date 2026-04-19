#pragma once

#include <cstdint>
#include <vector>

#include "sketch/SketchConstraint.hpp"
#include "sketch/SketchEntity.hpp"

namespace sketch {

/**
 * @brief Result of a sketch solve pass.
 */
struct SolveResult {
    bool converged = false;
    int dof = 0;
    int iterations = 0;
    float max_residual = 0.0f;
};

/**
 * @brief Incremental Newton-Raphson style sketch solver.
 */
class SketchSolver {
public:
    /**
     * @brief Solves a sketch incrementally.
     * @param entities Mutable entities.
     * @param constraints Constraint list.
     * @param max_iterations Iteration cap.
     * @return Solve metrics and DOF value.
     */
    SolveResult solve_incremental(
        std::vector<SketchEntity>& entities,
        const std::vector<SketchConstraint>& constraints,
        uint32_t max_iterations = 24U) const;

    /**
     * @brief Estimates the sketch DOF from variable and constraint counts.
     * @param entities Entity list.
     * @param constraints Constraint list.
     * @return Positive for under-constrained, zero for fully constrained, negative for over-constrained.
     */
    int estimate_dof(
        const std::vector<SketchEntity>& entities,
        const std::vector<SketchConstraint>& constraints) const;
};

}  // namespace sketch
