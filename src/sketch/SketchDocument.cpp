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
    const entity_id id = next_entity_id_++;
    entities_.push_back(SketchEntity{id, construction, false, LineEntity{p1, p2}});
    return id;
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
    last_result_ = solver_.solve_incremental(entities_, constraints_);
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

}  // namespace sketch
