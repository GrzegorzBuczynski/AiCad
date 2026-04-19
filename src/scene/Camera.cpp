#include "scene/Camera.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace scene {

namespace {

constexpr float k_orbit_sensitivity = 0.005f;
constexpr float k_pan_sensitivity = 0.0025f;
constexpr float k_dolly_sensitivity = 0.15f;
constexpr float k_zoom_sensitivity = 0.01f;

float clamp_fov(float value) {
    return std::clamp(value, 10.0f, 120.0f);
}

glm::vec3 safe_normalize(const glm::vec3& value, const glm::vec3& fallback) {
    const float length_sq = glm::dot(value, value);
    if (length_sq < 1.0e-8f) {
        return fallback;
    }
    return value / std::sqrt(length_sq);
}

}  // namespace

Camera::Camera() {
    set_isometric_view();
}

bool Camera::handle_event(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
        if (!middle_button_down_ && !right_button_down_) {
            return false;
        }

        if (middle_button_down_) {
            const float delta_x = static_cast<float>(event.motion.x - last_mouse_x_);
            const float delta_y = static_cast<float>(event.motion.y - last_mouse_y_);
            if (shift_down_) {
                pan(delta_x, delta_y);
            } else {
                orbit(delta_x, delta_y);
            }
        } else if (right_button_down_) {
            const float delta_y = static_cast<float>(event.motion.y - last_mouse_y_);
            zoom_vertical(delta_y);
        }

        last_mouse_x_ = event.motion.x;
        last_mouse_y_ = event.motion.y;
        return true;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_MIDDLE) {
            middle_button_down_ = true;
            last_mouse_x_ = event.button.x;
            last_mouse_y_ = event.button.y;
            return true;
        }
        if (event.button.button == SDL_BUTTON_RIGHT) {
            right_button_down_ = true;
            last_mouse_x_ = event.button.x;
            last_mouse_y_ = event.button.y;
            return true;
        }
        return false;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_MIDDLE) {
            middle_button_down_ = false;
            return true;
        }
        if (event.button.button == SDL_BUTTON_RIGHT) {
            right_button_down_ = false;
            return true;
        }
        return false;

    case SDL_EVENT_MOUSE_WHEEL:
        dolly(static_cast<float>(event.wheel.y) * k_dolly_sensitivity);
        return true;

    case SDL_EVENT_KEY_DOWN: {
        const SDL_Keycode key = event.key.key;
        shift_down_ = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;

        if (key == SDLK_V) {
            state_.projection_mode = (state_.projection_mode == ProjectionMode::perspective)
                ? ProjectionMode::orthographic
                : ProjectionMode::perspective;
            return true;
        }
        if (key == SDLK_F) {
            fit_all({glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{1.0f, 1.0f, 1.0f}});
            return true;
        }
        if (key == SDLK_KP_1) {
            set_front_view();
            return true;
        }
        if (key == SDLK_KP_2) {
            set_top_view();
            return true;
        }
        if (key == SDLK_KP_3) {
            set_right_view();
            return true;
        }
        if (key == SDLK_KP_0) {
            set_isometric_view();
            return true;
        }
        return false;
    }

    case SDL_EVENT_KEY_UP:
        shift_down_ = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
        return false;

    default:
        return false;
    }
}

const State& Camera::state() const {
    return state_;
}

void Camera::set_state(const State& new_state) {
    state_ = new_state;
    state_.up = safe_normalize(state_.up, glm::vec3{0.0f, 1.0f, 0.0f});
    state_.fov = clamp_fov(state_.fov);
}

glm::mat4 Camera::view_matrix() const {
    return glm::lookAt(state_.position, state_.target, up_direction());
}

glm::mat4 Camera::projection_matrix(float aspect_ratio) const {
    const float safe_aspect = std::max(aspect_ratio, 0.001f);
    if (state_.projection_mode == ProjectionMode::perspective) {
        return glm::perspective(glm::radians(state_.fov), safe_aspect, state_.near_plane, state_.far_plane);
    }

    const float scale = std::max(state_.ortho_scale, 0.001f);
    const float half_height = scale * 0.5f;
    const float half_width = half_height * safe_aspect;
    return glm::ortho(
        -half_width,
        half_width,
        -half_height,
        half_height,
        state_.near_plane,
        state_.far_plane);
}

void Camera::set_viewport_size(float width, float height) {
    viewport_width_ = std::max(width, 1.0f);
    viewport_height_ = std::max(height, 1.0f);
}

void Camera::fit_all(const Aabb& bounds) {
    const glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
    const glm::vec3 extents = bounds.max - bounds.min;
    const float radius = std::max({extents.x, extents.y, extents.z}) * 0.5f;
    const float distance = std::max(radius * 2.5f, 1.0f);

    state_.target = center;
    state_.position = center + glm::vec3{distance, distance, distance};
    state_.projection_mode = ProjectionMode::perspective;
}

nlohmann::json Camera::to_json() const {
    return nlohmann::json{
        {"position", {state_.position.x, state_.position.y, state_.position.z}},
        {"target", {state_.target.x, state_.target.y, state_.target.z}},
        {"up", {state_.up.x, state_.up.y, state_.up.z}},
        {"fov", state_.fov},
        {"near_plane", state_.near_plane},
        {"far_plane", state_.far_plane},
        {"ortho_scale", state_.ortho_scale},
        {"projection_mode", state_.projection_mode == ProjectionMode::perspective ? "perspective" : "orthographic"},
    };
}

bool Camera::from_json(const nlohmann::json& json_state) {
    if (!json_state.is_object()) {
        return false;
    }

    const auto read_vec3 = [&json_state](const char* key, glm::vec3& output) -> bool {
        if (!json_state.contains(key) || !json_state.at(key).is_array() || json_state.at(key).size() != 3) {
            return false;
        }

        output.x = json_state.at(key).at(0).get<float>();
        output.y = json_state.at(key).at(1).get<float>();
        output.z = json_state.at(key).at(2).get<float>();
        return true;
    };

    State restored = state_;
    if (!read_vec3("position", restored.position)) {
        return false;
    }
    if (!read_vec3("target", restored.target)) {
        return false;
    }
    if (!read_vec3("up", restored.up)) {
        return false;
    }

    if (json_state.contains("fov")) {
        restored.fov = json_state.at("fov").get<float>();
    }
    if (json_state.contains("near_plane")) {
        restored.near_plane = json_state.at("near_plane").get<float>();
    }
    if (json_state.contains("far_plane")) {
        restored.far_plane = json_state.at("far_plane").get<float>();
    }
    if (json_state.contains("ortho_scale")) {
        restored.ortho_scale = json_state.at("ortho_scale").get<float>();
    }
    if (json_state.contains("projection_mode")) {
        const std::string mode = json_state.at("projection_mode").get<std::string>();
        restored.projection_mode = (mode == "orthographic") ? ProjectionMode::orthographic : ProjectionMode::perspective;
    }

    set_state(restored);
    return true;
}

bool Camera::is_interacting() const {
    return middle_button_down_ || right_button_down_;
}

void Camera::orbit(float delta_x, float delta_y) {
    const glm::vec3 offset = state_.position - state_.target;
    glm::quat yaw = glm::angleAxis(-delta_x * k_orbit_sensitivity, up_direction());
    glm::quat pitch = glm::angleAxis(-delta_y * k_orbit_sensitivity, right());
    const glm::vec3 rotated = pitch * yaw * offset;

    state_.position = state_.target + rotated;
}

void Camera::pan(float delta_x, float delta_y) {
    const glm::vec3 pan_offset = (-right() * delta_x + up_direction() * delta_y) * k_pan_sensitivity * glm::length(state_.position - state_.target);
    state_.position += pan_offset;
    state_.target += pan_offset;
}

void Camera::dolly(float amount) {
    const glm::vec3 direction = forward();
    const float distance = glm::length(state_.position - state_.target);
    const float new_distance = std::max(distance * (1.0f - amount), 0.1f);
    state_.position = state_.target - direction * new_distance;
}

void Camera::zoom_vertical(float amount) {
    if (state_.projection_mode == ProjectionMode::perspective) {
        state_.fov = clamp_fov(state_.fov + amount * k_zoom_sensitivity);
    } else {
        state_.ortho_scale = std::max(state_.ortho_scale + amount * k_zoom_sensitivity, 0.001f);
    }
}

void Camera::set_front_view() {
    state_.projection_mode = ProjectionMode::orthographic;
    state_.position = state_.target + glm::vec3{0.0f, 0.0f, 10.0f};
    state_.up = {0.0f, 1.0f, 0.0f};
}

void Camera::set_top_view() {
    state_.projection_mode = ProjectionMode::orthographic;
    state_.position = state_.target + glm::vec3{0.0f, 10.0f, 0.0f};
    state_.up = {0.0f, 0.0f, -1.0f};
}

void Camera::set_right_view() {
    state_.projection_mode = ProjectionMode::orthographic;
    state_.position = state_.target + glm::vec3{10.0f, 0.0f, 0.0f};
    state_.up = {0.0f, 1.0f, 0.0f};
}

void Camera::set_isometric_view() {
    state_.projection_mode = ProjectionMode::perspective;
    state_.position = state_.target + glm::vec3{6.0f, 6.0f, 6.0f};
    state_.up = {0.0f, 1.0f, 0.0f};
    state_.fov = 60.0f;
}

glm::vec3 Camera::forward() const {
    return safe_normalize(state_.target - state_.position, glm::vec3{0.0f, 0.0f, -1.0f});
}

glm::vec3 Camera::right() const {
    return safe_normalize(glm::cross(forward(), up_direction()), glm::vec3{1.0f, 0.0f, 0.0f});
}

glm::vec3 Camera::up_direction() const {
    return safe_normalize(state_.up, glm::vec3{0.0f, 1.0f, 0.0f});
}

}  // namespace scene
