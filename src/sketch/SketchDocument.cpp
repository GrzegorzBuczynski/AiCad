#include "sketch/SketchDocument.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <glm/gtc/constants.hpp>

namespace sketch {

namespace {

bool almost_equal(const glm::vec2& a, const glm::vec2& b, float eps = 1.0e-3f) {
    return glm::length(a - b) <= eps;
}

}  // namespace

SketchDocument::SketchDocument(const glm::vec3& feature_point)
    : plane_(feature_point) {}

void SketchDocument::enter() {
    active_ = true;
}

void SketchDocument::exit() {
    active_ = false;
}

bool SketchDocument::is_active() const {
    return active_;
}

entity_id SketchDocument::add_point(const glm::vec2& pos, bool construction) {
    const entity_id id = next_entity_id_++;
    entities_.push_back(SketchEntity{id, construction, false, PointEntity{pos}});
    return id;
}

entity_id SketchDocument::add_line(const glm::vec2& p1, const glm::vec2& p2, bool construction) {
    const entity_id point_a_id = add_point(p1, construction);
    const entity_id point_b_id = add_point(p2, construction);

    const entity_id id = next_entity_id_++;
    entities_.push_back(SketchEntity{id, construction, false, LineEntity{point_a_id, point_b_id, p1, p2}});
    return id;
}

std::optional<std::pair<entity_id, entity_id>> SketchDocument::line_points(entity_id line_id) const {
    const SketchEntity* entity = find_entity(line_id);
    if (entity == nullptr) {
        return std::nullopt;
    }

    const auto* line = std::get_if<LineEntity>(&entity->data);
    if (line == nullptr) {
        return std::nullopt;
    }

    return std::make_pair(line->point_a, line->point_b);
}

entity_id SketchDocument::add_circle(const glm::vec2& center, float radius, bool construction) {
    const entity_id id = next_entity_id_++;
    entities_.push_back(SketchEntity{id, construction, false, CircleEntity{center, std::max(radius, 0.001f)}});
    return id;
}

entity_id SketchDocument::add_arc(const glm::vec2& center, float radius, float angle_start, float angle_end, bool construction) {
    const entity_id id = next_entity_id_++;
    entities_.push_back(SketchEntity{id, construction, false, ArcEntity{center, std::max(radius, 0.001f), angle_start, angle_end}});
    return id;
}

constraint_id SketchDocument::add_constraint(const SketchConstraintData& data) {
    const constraint_id id = next_constraint_id_++;
    constraints_.push_back(SketchConstraint{id, data});
    return id;
}

SolveResult SketchDocument::solve() {
    sync_lines_from_points();
    last_result_ = solver_.solve_incremental(entities_, constraints_);
    sync_points_from_lines();
    sync_lines_from_points();
    return last_result_;
}

std::vector<geometry::Profile> SketchDocument::extract_profiles() const {
    std::vector<geometry::Profile> profiles{};

    struct Segment {
        glm::vec2 a{};
        glm::vec2 b{};
        bool used = false;
    };

    std::vector<Segment> segments{};
    for (const SketchEntity& entity : entities_) {
        if (entity.construction) {
            continue;
        }

        if (const auto* line = std::get_if<LineEntity>(&entity.data)) {
            segments.push_back({line->p1, line->p2, false});
        } else if (const auto* circle = std::get_if<CircleEntity>(&entity.data)) {
            geometry::Profile profile{};
            profile.closed = true;
            constexpr int steps = 32;
            profile.points.reserve(steps);
            for (int i = 0; i < steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const float ang = t * glm::two_pi<float>();
                profile.points.push_back(circle->center + glm::vec2(std::cos(ang), std::sin(ang)) * circle->radius);
            }
            profiles.push_back(std::move(profile));
        }
    }

    for (Segment& segment : segments) {
        if (segment.used) {
            continue;
        }

        geometry::Profile profile{};
        profile.closed = false;
        profile.points.push_back(segment.a);
        profile.points.push_back(segment.b);
        segment.used = true;

        glm::vec2 current = segment.b;
        while (true) {
            bool extended = false;
            for (Segment& next : segments) {
                if (next.used) {
                    continue;
                }

                if (almost_equal(next.a, current)) {
                    profile.points.push_back(next.b);
                    current = next.b;
                    next.used = true;
                    extended = true;
                    break;
                }
                if (almost_equal(next.b, current)) {
                    profile.points.push_back(next.a);
                    current = next.a;
                    next.used = true;
                    extended = true;
                    break;
                }
            }

            if (!extended) {
                break;
            }

            if (almost_equal(profile.points.front(), current)) {
                profile.closed = true;
                break;
            }
        }

        if (profile.closed && profile.points.size() >= 3U) {
            if (almost_equal(profile.points.front(), profile.points.back())) {
                profile.points.pop_back();
            }
            profiles.push_back(std::move(profile));
        }
    }

    return profiles;
}

std::vector<geometry::Profile> SketchDocument::extract_preview_profiles() const {
    std::vector<geometry::Profile> preview{};

    for (const SketchEntity& entity : entities_) {
        if (entity.construction) {
            continue;
        }

        if (const auto* line = std::get_if<LineEntity>(&entity.data)) {
            geometry::Profile curve{};
            curve.closed = false;
            curve.points = {line->p1, line->p2};
            preview.push_back(std::move(curve));
            continue;
        }

        if (const auto* circle = std::get_if<CircleEntity>(&entity.data)) {
            geometry::Profile curve{};
            curve.closed = true;
            constexpr int steps = 64;
            curve.points.reserve(steps);
            for (int i = 0; i < steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const float ang = t * glm::two_pi<float>();
                curve.points.push_back(circle->center + glm::vec2(std::cos(ang), std::sin(ang)) * circle->radius);
            }
            preview.push_back(std::move(curve));
            continue;
        }

        if (const auto* arc = std::get_if<ArcEntity>(&entity.data)) {
            geometry::Profile curve{};
            curve.closed = false;
            constexpr int steps = 48;
            const float start_rad = glm::radians(arc->angle_start);
            const float end_rad = glm::radians(arc->angle_end);
            float span = end_rad - start_rad;
            if (span <= 0.0f) {
                span += glm::two_pi<float>();
            }

            curve.points.reserve(steps + 1);
            for (int i = 0; i <= steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const float a = start_rad + span * t;
                curve.points.push_back(arc->center + glm::vec2(std::cos(a), std::sin(a)) * arc->r);
            }
            preview.push_back(std::move(curve));
            continue;
        }

        if (const auto* point = std::get_if<PointEntity>(&entity.data)) {
            geometry::Profile curve{};
            curve.closed = false;
            curve.points = {point->pos};
            preview.push_back(std::move(curve));
        }
    }

    const std::vector<geometry::Profile> closed = extract_profiles();
    preview.insert(preview.end(), closed.begin(), closed.end());
    return preview;
}

std::unordered_set<entity_id> SketchDocument::constrained_entities() const {
    std::unordered_set<entity_id> ids{};
    for (const SketchConstraint& constraint : constraints_) {
        std::visit([&ids](const auto& typed) {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, Coincident>) {
                ids.insert(typed.entity_a);
                ids.insert(typed.entity_b);
            } else if constexpr (std::is_same_v<T, Parallel> || std::is_same_v<T, Perpendicular> || std::is_same_v<T, EqualLength> || std::is_same_v<T, AngleDim>) {
                ids.insert(typed.line_a);
                ids.insert(typed.line_b);
            } else if constexpr (std::is_same_v<T, Tangent>) {
                ids.insert(typed.entity_a);
                ids.insert(typed.entity_b);
            } else if constexpr (std::is_same_v<T, FixedPoint>) {
                ids.insert(typed.entity);
            } else if constexpr (std::is_same_v<T, HorizontalConstraint> || std::is_same_v<T, VerticalConstraint>) {
                ids.insert(typed.line);
            } else if constexpr (std::is_same_v<T, DistanceDim>) {
                ids.insert(typed.entity_a);
                ids.insert(typed.entity_b);
            } else if constexpr (std::is_same_v<T, RadiusDim>) {
                ids.insert(typed.entity);
            }
        }, constraint.data);
    }
    return ids;
}

SketchEntity* SketchDocument::find_entity(entity_id id) {
    for (SketchEntity& entity : entities_) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

const SketchEntity* SketchDocument::find_entity(entity_id id) const {
    for (const SketchEntity& entity : entities_) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

const std::vector<SketchEntity>& SketchDocument::entities() const {
    return entities_;
}

const std::vector<SketchConstraint>& SketchDocument::constraints() const {
    return constraints_;
}

int SketchDocument::dof() const {
    return solver_.estimate_dof(entities_, constraints_);
}

const SolveResult& SketchDocument::last_solve_result() const {
    return last_result_;
}

const Plane& SketchDocument::plane() const {
    return plane_;
}

void SketchDocument::set_plane(const Plane& plane) {
    plane_ = plane;
    const float len = glm::length(plane_.normal);
    if (len < 1.0e-6f) {
        plane_.normal = {0.0f, 0.0f, 1.0f};
    } else {
        plane_.normal /= len;
    }

    if (!grid_features_.empty()) {
        grid_features_.front().plane = plane_;
    }
}

uint32_t SketchDocument::add_grid_feature_on_plane() {
    if (!grid_features_.empty()) {
        return grid_features_.front().id;
    }

    const uint32_t new_id = next_grid_feature_id_++;
    grid_features_.push_back(GridFeature{new_id, plane_, 1.0f, 10.0f, 60, true});
    return new_id;
}

bool SketchDocument::has_grid_feature() const {
    return !grid_features_.empty();
}

const GridFeature* SketchDocument::active_grid_feature() const {
    if (grid_features_.empty()) {
        return nullptr;
    }
    return &grid_features_.front();
}

GridFeature* SketchDocument::active_grid_feature() {
    if (grid_features_.empty()) {
        return nullptr;
    }
    return &grid_features_.front();
}

bool SketchDocument::snap_enabled() const {
    return snap_enabled_;
}

void SketchDocument::set_snap_enabled(bool enabled) {
    snap_enabled_ = enabled;
}

void SketchDocument::sync_lines_from_points() {
    for (SketchEntity& entity : entities_) {
        auto* line = std::get_if<LineEntity>(&entity.data);
        if (line == nullptr) {
            continue;
        }

        const SketchEntity* point_a_entity = find_entity(line->point_a);
        const SketchEntity* point_b_entity = find_entity(line->point_b);
        if (point_a_entity == nullptr || point_b_entity == nullptr) {
            continue;
        }

        const auto* point_a = std::get_if<PointEntity>(&point_a_entity->data);
        const auto* point_b = std::get_if<PointEntity>(&point_b_entity->data);
        if (point_a == nullptr || point_b == nullptr) {
            continue;
        }

        line->p1 = point_a->pos;
        line->p2 = point_b->pos;
    }
}

void SketchDocument::sync_points_from_lines() {
    for (const SketchEntity& entity : entities_) {
        const auto* line = std::get_if<LineEntity>(&entity.data);
        if (line == nullptr) {
            continue;
        }

        SketchEntity* point_a_entity = find_entity(line->point_a);
        SketchEntity* point_b_entity = find_entity(line->point_b);
        if (point_a_entity == nullptr || point_b_entity == nullptr) {
            continue;
        }

        auto* point_a = std::get_if<PointEntity>(&point_a_entity->data);
        auto* point_b = std::get_if<PointEntity>(&point_b_entity->data);
        if (point_a == nullptr || point_b == nullptr) {
            continue;
        }

        point_a->pos = line->p1;
        point_b->pos = line->p2;
    }
}

}  // namespace sketch
