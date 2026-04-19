#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

namespace sketch {

using entity_id = uint32_t;

/**
 * @brief Sketch point entity.
 */
struct PointEntity {
    glm::vec2 pos{0.0f, 0.0f};
};

/**
 * @brief Sketch line entity.
 */
struct LineEntity {
    glm::vec2 p1{0.0f, 0.0f};
    glm::vec2 p2{10.0f, 0.0f};
};

/**
 * @brief Sketch arc entity.
 */
struct ArcEntity {
    glm::vec2 center{0.0f, 0.0f};
    float r = 10.0f;
    float angle_start = 0.0f;
    float angle_end = 90.0f;
};

/**
 * @brief Sketch circle entity.
 */
struct CircleEntity {
    glm::vec2 center{0.0f, 0.0f};
    float radius = 10.0f;
};

using SketchEntityData = std::variant<PointEntity, LineEntity, ArcEntity, CircleEntity>;

/**
 * @brief Generic sketch entity record.
 */
struct SketchEntity {
    entity_id id = 0U;
    bool construction = false;
    bool selected = false;
    SketchEntityData data{};
};

/**
 * @brief Returns all editable points for an entity.
 * @param entity Entity to inspect.
 * @return Point list in local order.
 */
std::vector<glm::vec2> control_points(const SketchEntity& entity);

/**
 * @brief Gets a specific point from an entity.
 * @param entity Entity to inspect.
 * @param point_index Point index.
 * @return Point when available.
 */
std::optional<glm::vec2> get_point(const SketchEntity& entity, uint32_t point_index);

/**
 * @brief Writes a specific point of an entity.
 * @param entity Entity to modify.
 * @param point_index Point index.
 * @param value New point value.
 * @return True when update succeeded.
 */
bool set_point(SketchEntity* entity, uint32_t point_index, const glm::vec2& value);

/**
 * @brief Returns canonical radius for circle/arc entities.
 * @param entity Entity to inspect.
 * @return Radius when available.
 */
std::optional<float> get_radius(const SketchEntity& entity);

/**
 * @brief Writes radius for circle/arc entities.
 * @param entity Entity to modify.
 * @param radius New radius.
 * @return True when update succeeded.
 */
bool set_radius(SketchEntity* entity, float radius);

/**
 * @brief Returns line segment length for line entities.
 * @param entity Entity to inspect.
 * @return Length when entity is a line.
 */
std::optional<float> line_length(const SketchEntity& entity);

}  // namespace sketch
