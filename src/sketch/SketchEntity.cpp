#include "sketch/SketchEntity.hpp"

#include <algorithm>
#include <cmath>

namespace sketch {

namespace {

glm::vec2 arc_endpoint(const ArcEntity& arc, float angle_deg) {
    const float rad = glm::radians(angle_deg);
    return arc.center + glm::vec2(std::cos(rad), std::sin(rad)) * arc.r;
}

float arc_angle_from_point(const ArcEntity& arc, const glm::vec2& point) {
    const glm::vec2 v = point - arc.center;
    return glm::degrees(std::atan2(v.y, v.x));
}

}  // namespace

std::vector<glm::vec2> control_points(const SketchEntity& entity) {
    if (const auto* point = std::get_if<PointEntity>(&entity.data)) {
        return {point->pos};
    }
    if (const auto* line = std::get_if<LineEntity>(&entity.data)) {
        return {line->p1, line->p2};
    }
    if (const auto* arc = std::get_if<ArcEntity>(&entity.data)) {
        return {arc_endpoint(*arc, arc->angle_start), arc_endpoint(*arc, arc->angle_end), arc->center};
    }
    if (const auto* circle = std::get_if<CircleEntity>(&entity.data)) {
        return {circle->center};
    }
    return {};
}

std::optional<glm::vec2> get_point(const SketchEntity& entity, uint32_t point_index) {
    if (const auto* point = std::get_if<PointEntity>(&entity.data)) {
        if (point_index == 0U) {
            return point->pos;
        }
        return std::nullopt;
    }

    if (const auto* line = std::get_if<LineEntity>(&entity.data)) {
        if (point_index == 0U) {
            return line->p1;
        }
        if (point_index == 1U) {
            return line->p2;
        }
        return std::nullopt;
    }

    if (const auto* arc = std::get_if<ArcEntity>(&entity.data)) {
        if (point_index == 0U) {
            return arc_endpoint(*arc, arc->angle_start);
        }
        if (point_index == 1U) {
            return arc_endpoint(*arc, arc->angle_end);
        }
        if (point_index == 2U) {
            return arc->center;
        }
        return std::nullopt;
    }

    if (const auto* circle = std::get_if<CircleEntity>(&entity.data)) {
        if (point_index == 0U) {
            return circle->center;
        }
        return std::nullopt;
    }

    return std::nullopt;
}

bool set_point(SketchEntity* entity, uint32_t point_index, const glm::vec2& value) {
    if (entity == nullptr) {
        return false;
    }

    if (auto* point = std::get_if<PointEntity>(&entity->data)) {
        if (point_index == 0U) {
            point->pos = value;
            return true;
        }
        return false;
    }

    if (auto* line = std::get_if<LineEntity>(&entity->data)) {
        if (point_index == 0U) {
            line->p1 = value;
            return true;
        }
        if (point_index == 1U) {
            line->p2 = value;
            return true;
        }
        return false;
    }

    if (auto* arc = std::get_if<ArcEntity>(&entity->data)) {
        if (point_index == 0U) {
            arc->angle_start = arc_angle_from_point(*arc, value);
            arc->r = std::max(0.001f, glm::length(value - arc->center));
            return true;
        }
        if (point_index == 1U) {
            arc->angle_end = arc_angle_from_point(*arc, value);
            arc->r = std::max(0.001f, glm::length(value - arc->center));
            return true;
        }
        if (point_index == 2U) {
            arc->center = value;
            return true;
        }
        return false;
    }

    if (auto* circle = std::get_if<CircleEntity>(&entity->data)) {
        if (point_index == 0U) {
            circle->center = value;
            return true;
        }
        return false;
    }

    return false;
}

std::optional<float> get_radius(const SketchEntity& entity) {
    if (const auto* circle = std::get_if<CircleEntity>(&entity.data)) {
        return std::max(circle->radius, 0.001f);
    }
    if (const auto* arc = std::get_if<ArcEntity>(&entity.data)) {
        return std::max(arc->r, 0.001f);
    }
    return std::nullopt;
}

bool set_radius(SketchEntity* entity, float radius) {
    if (entity == nullptr) {
        return false;
    }

    const float safe_radius = std::max(radius, 0.001f);
    if (auto* circle = std::get_if<CircleEntity>(&entity->data)) {
        circle->radius = safe_radius;
        return true;
    }
    if (auto* arc = std::get_if<ArcEntity>(&entity->data)) {
        arc->r = safe_radius;
        return true;
    }
    return false;
}

std::optional<float> line_length(const SketchEntity& entity) {
    const auto* line = std::get_if<LineEntity>(&entity.data);
    if (line == nullptr) {
        return std::nullopt;
    }
    return glm::length(line->p2 - line->p1);
}

}  // namespace sketch
