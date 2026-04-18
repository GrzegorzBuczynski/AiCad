#pragma once

#include <string>

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace scene {

/**
 * @brief Camera projection mode.
 */
enum class ProjectionMode {
    perspective,
    orthographic
};

/**
 * @brief Input state describing a scene bounds box.
 */
struct Aabb {
    glm::vec3 min{0.0f, 0.0f, 0.0f};
    glm::vec3 max{0.0f, 0.0f, 0.0f};
};

/**
 * @brief Serializable camera state for session restore.
 */
struct State {
    glm::vec3 position{0.0f, 0.0f, 8.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float ortho_scale = 1.0f;
    ProjectionMode projection_mode = ProjectionMode::perspective;
};

/**
 * @brief CATIA-like 3D camera controller.
 *
 * Middle mouse drag orbits around target.
 * Middle mouse + Shift pans.
 * Scroll wheel performs dolly zoom.
 * Right mouse vertical drag performs alternate zoom.
 * Keys 1/2/3 select orthographic front/top/right.
 * Key 0 selects isometric perspective.
 * Key V toggles perspective/orthographic.
 * Key F frames the current scene AABB.
 */
class Camera {
public:
    /**
     * @brief Constructs a camera with a default isometric-like view.
     */
    Camera();

    /**
     * @brief Processes an SDL event and updates the camera when relevant.
     * @param event SDL event to consume.
     * @return True when the event was handled.
     */
    bool handle_event(const SDL_Event& event);

    /**
     * @brief Returns current serializable state.
     * @return Camera state.
     */
    [[nodiscard]] const State& state() const;

    /**
     * @brief Replaces current state.
     * @param new_state Camera state to restore.
     */
    void set_state(const State& new_state);

    /**
     * @brief Returns view matrix.
     * @return 4x4 view matrix.
     */
    [[nodiscard]] glm::mat4 view_matrix() const;

    /**
     * @brief Returns projection matrix for given aspect ratio.
     * @param aspect_ratio Viewport aspect ratio.
     * @return 4x4 projection matrix.
     */
    [[nodiscard]] glm::mat4 projection_matrix(float aspect_ratio) const;

    /**
     * @brief Sets the current viewport size in pixels.
     * @param width Viewport width.
     * @param height Viewport height.
     */
    void set_viewport_size(float width, float height);

    /**
     * @brief Frames the given axis-aligned bounds.
     * @param bounds Scene bounds to fit.
     */
    void fit_all(const Aabb& bounds);

    /**
     * @brief Serializes the camera state to JSON.
     * @return JSON object containing camera state.
     */
    [[nodiscard]] nlohmann::json to_json() const;

    /**
     * @brief Restores the camera state from JSON.
     * @param json_state JSON object containing camera state.
     * @return True on success, false on malformed input.
     */
    bool from_json(const nlohmann::json& json_state);

private:
    void orbit(float delta_x, float delta_y);
    void pan(float delta_x, float delta_y);
    void dolly(float amount);
    void zoom_vertical(float amount);
    void set_front_view();
    void set_top_view();
    void set_right_view();
    void set_isometric_view();

    [[nodiscard]] glm::vec3 forward() const;
    [[nodiscard]] glm::vec3 right() const;
    [[nodiscard]] glm::vec3 up_direction() const;

    State state_{};
    bool middle_button_down_ = false;
    bool right_button_down_ = false;
    bool shift_down_ = false;
    int last_mouse_x_ = 0;
    int last_mouse_y_ = 0;
    float viewport_width_ = 1.0f;
    float viewport_height_ = 1.0f;
};

}  // namespace scene
