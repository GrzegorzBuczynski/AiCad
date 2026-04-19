#include "sketch/SketchSolver.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace sketch {

namespace {

SketchEntity* find_entity(std::vector<SketchEntity>& entities, entity_id id) {
    for (SketchEntity& entity : entities) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

const SketchEntity* find_entity_const(const std::vector<SketchEntity>& entities, entity_id id) {
    for (const SketchEntity& entity : entities) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

glm::vec2 normalize_or(const glm::vec2& value, const glm::vec2& fallback) {
    const float len = glm::length(value);
    if (len < 1.0e-6f) {
        return fallback;
    }
    return value / len;
}

float apply_constraint(std::vector<SketchEntity>& entities, const SketchConstraint& constraint) {
    return std::visit([&entities](const auto& typed) -> float {
        using T = std::decay_t<decltype(typed)>;

        if constexpr (std::is_same_v<T, Coincident>) {
            SketchEntity* a = find_entity(entities, typed.entity_a);
            SketchEntity* b = find_entity(entities, typed.entity_b);
            if (a == nullptr || b == nullptr) {
                return 0.0f;
            }

            const auto p_a = get_point(*a, typed.point_a);
            const auto p_b = get_point(*b, typed.point_b);
            if (!p_a.has_value() || !p_b.has_value()) {
                return 0.0f;
            }

            const glm::vec2 mid = (*p_a + *p_b) * 0.5f;
            set_point(a, typed.point_a, mid);
            set_point(b, typed.point_b, mid);
            return glm::length(*p_a - *p_b);
        }

        if constexpr (std::is_same_v<T, Parallel>) {
            SketchEntity* a = find_entity(entities, typed.line_a);
            SketchEntity* b = find_entity(entities, typed.line_b);
            if (a == nullptr || b == nullptr) {
                return 0.0f;
            }

            const auto a0 = get_point(*a, 0U);
            const auto a1 = get_point(*a, 1U);
            const auto b0 = get_point(*b, 0U);
            const auto b1 = get_point(*b, 1U);
            if (!a0.has_value() || !a1.has_value() || !b0.has_value() || !b1.has_value()) {
                return 0.0f;
            }

            const glm::vec2 dir_a = normalize_or(*a1 - *a0, glm::vec2{1.0f, 0.0f});
            const glm::vec2 center_b = (*b0 + *b1) * 0.5f;
            const float len_b = glm::length(*b1 - *b0);
            const glm::vec2 half = dir_a * len_b * 0.5f;
            set_point(b, 0U, center_b - half);
            set_point(b, 1U, center_b + half);

            const glm::vec2 dir_b = normalize_or(*b1 - *b0, glm::vec2{1.0f, 0.0f});
            return std::abs(1.0f - std::abs(glm::dot(dir_a, dir_b)));
        }

        if constexpr (std::is_same_v<T, Perpendicular>) {
            SketchEntity* a = find_entity(entities, typed.line_a);
            SketchEntity* b = find_entity(entities, typed.line_b);
            if (a == nullptr || b == nullptr) {
                return 0.0f;
            }

            const auto a0 = get_point(*a, 0U);
            const auto a1 = get_point(*a, 1U);
            const auto b0 = get_point(*b, 0U);
            const auto b1 = get_point(*b, 1U);
            if (!a0.has_value() || !a1.has_value() || !b0.has_value() || !b1.has_value()) {
                return 0.0f;
            }

            const glm::vec2 dir_a = normalize_or(*a1 - *a0, glm::vec2{1.0f, 0.0f});
            const glm::vec2 perp{-dir_a.y, dir_a.x};
            const glm::vec2 center_b = (*b0 + *b1) * 0.5f;
            const float len_b = glm::length(*b1 - *b0);
            const glm::vec2 half = perp * len_b * 0.5f;
            set_point(b, 0U, center_b - half);
            set_point(b, 1U, center_b + half);

            const glm::vec2 dir_b = normalize_or(*b1 - *b0, glm::vec2{0.0f, 1.0f});
            return std::abs(glm::dot(dir_a, dir_b));
        }

        if constexpr (std::is_same_v<T, Tangent>) {
            const SketchEntity* a = find_entity_const(entities, typed.entity_a);
            const SketchEntity* b = find_entity_const(entities, typed.entity_b);
            if (a == nullptr || b == nullptr) {
                return 0.0f;
            }

            const auto r_a = get_radius(*a);
            const auto r_b = get_radius(*b);
            const auto p_a = get_point(*a, 0U);
            const auto p_b = get_point(*b, 0U);
            if (!r_a.has_value() || !r_b.has_value() || !p_a.has_value() || !p_b.has_value()) {
                return 0.0f;
            }

            return std::abs(glm::length(*p_b - *p_a) - (*r_a + *r_b));
        }

        if constexpr (std::is_same_v<T, EqualLength>) {
            SketchEntity* a = find_entity(entities, typed.line_a);
            SketchEntity* b = find_entity(entities, typed.line_b);
            if (a == nullptr || b == nullptr) {
                return 0.0f;
            }

            const auto length_a = line_length(*a);
            const auto b0 = get_point(*b, 0U);
            const auto b1 = get_point(*b, 1U);
            if (!length_a.has_value() || !b0.has_value() || !b1.has_value()) {
                return 0.0f;
            }

            const glm::vec2 center = (*b0 + *b1) * 0.5f;
            const glm::vec2 dir = normalize_or(*b1 - *b0, glm::vec2{1.0f, 0.0f});
            const glm::vec2 half = dir * (*length_a) * 0.5f;
            set_point(b, 0U, center - half);
            set_point(b, 1U, center + half);

            const auto length_b = line_length(*b);
            return std::abs(length_b.value_or(*length_a) - *length_a);
        }

        if constexpr (std::is_same_v<T, FixedPoint>) {
            SketchEntity* entity = find_entity(entities, typed.entity);
            if (entity == nullptr) {
                return 0.0f;
            }

            const auto p = get_point(*entity, typed.point);
            if (!p.has_value()) {
                return 0.0f;
            }

            set_point(entity, typed.point, typed.value);
            return glm::length(*p - typed.value);
        }

        if constexpr (std::is_same_v<T, HorizontalConstraint>) {
            SketchEntity* line = find_entity(entities, typed.line);
            if (line == nullptr) {
                return 0.0f;
            }

            const auto p1 = get_point(*line, 0U);
            const auto p2 = get_point(*line, 1U);
            if (!p1.has_value() || !p2.has_value()) {
                return 0.0f;
            }

            const float y = (p1->y + p2->y) * 0.5f;
            set_point(line, 0U, {p1->x, y});
            set_point(line, 1U, {p2->x, y});
            return std::abs(p1->y - p2->y);
        }

        if constexpr (std::is_same_v<T, VerticalConstraint>) {
            SketchEntity* line = find_entity(entities, typed.line);
            if (line == nullptr) {
                return 0.0f;
            }

            const auto p1 = get_point(*line, 0U);
            const auto p2 = get_point(*line, 1U);
            if (!p1.has_value() || !p2.has_value()) {
                return 0.0f;
            }

            const float x = (p1->x + p2->x) * 0.5f;
            set_point(line, 0U, {x, p1->y});
            set_point(line, 1U, {x, p2->y});
            return std::abs(p1->x - p2->x);
        }

        if constexpr (std::is_same_v<T, DistanceDim>) {
            SketchEntity* a = find_entity(entities, typed.entity_a);
            SketchEntity* b = find_entity(entities, typed.entity_b);
            if (a == nullptr || b == nullptr) {
                return 0.0f;
            }

            const auto p_a = get_point(*a, typed.point_a);
            const auto p_b = get_point(*b, typed.point_b);
            if (!p_a.has_value() || !p_b.has_value()) {
                return 0.0f;
            }

            const glm::vec2 delta = *p_b - *p_a;
            const float length = glm::length(delta);
            const glm::vec2 dir = normalize_or(delta, glm::vec2{1.0f, 0.0f});
            const float target = std::max(typed.value, 0.001f);
            const glm::vec2 center = (*p_a + *p_b) * 0.5f;
            const glm::vec2 half = dir * target * 0.5f;
            set_point(a, typed.point_a, center - half);
            set_point(b, typed.point_b, center + half);
            return std::abs(length - target);
        }

        if constexpr (std::is_same_v<T, AngleDim>) {
            SketchEntity* a = find_entity(entities, typed.line_a);
            SketchEntity* b = find_entity(entities, typed.line_b);
            if (a == nullptr || b == nullptr) {
                return 0.0f;
            }

            const auto a0 = get_point(*a, 0U);
            const auto a1 = get_point(*a, 1U);
            const auto b0 = get_point(*b, 0U);
            const auto b1 = get_point(*b, 1U);
            if (!a0.has_value() || !a1.has_value() || !b0.has_value() || !b1.has_value()) {
                return 0.0f;
            }

            const glm::vec2 dir_a = normalize_or(*a1 - *a0, glm::vec2{1.0f, 0.0f});
            const float angle_rad = glm::radians(typed.degrees);
            const glm::mat2 rot(
                std::cos(angle_rad), -std::sin(angle_rad),
                std::sin(angle_rad), std::cos(angle_rad));
            const glm::vec2 desired_dir = normalize_or(rot * dir_a, glm::vec2{0.0f, 1.0f});
            const glm::vec2 center_b = (*b0 + *b1) * 0.5f;
            const float len_b = glm::length(*b1 - *b0);
            const glm::vec2 half = desired_dir * len_b * 0.5f;
            set_point(b, 0U, center_b - half);
            set_point(b, 1U, center_b + half);

            const glm::vec2 dir_b = normalize_or(*b1 - *b0, glm::vec2{0.0f, 1.0f});
            const float current_angle = std::acos(std::clamp(glm::dot(dir_a, dir_b), -1.0f, 1.0f));
            return std::abs(current_angle - angle_rad);
        }

        if constexpr (std::is_same_v<T, RadiusDim>) {
            SketchEntity* entity = find_entity(entities, typed.entity);
            if (entity == nullptr) {
                return 0.0f;
            }

            const auto current = get_radius(*entity);
            if (!current.has_value()) {
                return 0.0f;
            }

            const float target = std::max(typed.value, 0.001f);
            set_radius(entity, target);
            return std::abs(*current - target);
        }

        return 0.0f;
    }, constraint.data);
}

int count_entity_parameters(const SketchEntity& entity) {
    if (std::holds_alternative<PointEntity>(entity.data)) {
        return 2;
    }
    if (std::holds_alternative<LineEntity>(entity.data)) {
        return 4;
    }
    if (std::holds_alternative<ArcEntity>(entity.data)) {
        return 5;
    }
    if (std::holds_alternative<CircleEntity>(entity.data)) {
        return 3;
    }
    return 0;
}

int count_constraint_equations(const SketchConstraint& constraint) {
    return std::visit([](const auto& typed) -> int {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, Coincident>) {
            return 2;
        }
        if constexpr (std::is_same_v<T, FixedPoint>) {
            return 2;
        }
        return 1;
    }, constraint.data);
}

}  // namespace

SolveResult SketchSolver::solve_incremental(
    std::vector<SketchEntity>& entities,
    const std::vector<SketchConstraint>& constraints,
    uint32_t max_iterations) const {
    SolveResult result{};
    result.dof = estimate_dof(entities, constraints);

    constexpr float convergence_eps = 1.0e-3f;
    for (uint32_t iteration = 0U; iteration < max_iterations; ++iteration) {
        float max_residual = 0.0f;
        for (const SketchConstraint& constraint : constraints) {
            const float residual = apply_constraint(entities, constraint);
            max_residual = std::max(max_residual, residual);
        }

        result.iterations = static_cast<int>(iteration + 1U);
        result.max_residual = max_residual;

        if (max_residual <= convergence_eps) {
            result.converged = true;
            break;
        }
    }

    return result;
}

int SketchSolver::estimate_dof(
    const std::vector<SketchEntity>& entities,
    const std::vector<SketchConstraint>& constraints) const {
    int parameter_count = 0;
    for (const SketchEntity& entity : entities) {
        parameter_count += count_entity_parameters(entity);
    }

    int equation_count = 0;
    for (const SketchConstraint& constraint : constraints) {
        equation_count += count_constraint_equations(constraint);
    }

    return parameter_count - equation_count;
}

}  // namespace sketch
