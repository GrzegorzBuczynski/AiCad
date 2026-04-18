#pragma once

#include <cstdint>
#include <string>

#include "app/Window.hpp"
#include "renderer/RenderFrame.hpp"
#include "renderer/VulkanContext.hpp"

namespace app {

/**
 * @brief Runtime configuration loaded from settings.json.
 */
struct app_settings {
    uint32_t width = 1600;
    uint32_t height = 900;
    std::string renderer = "vulkan";
    bool vsync = true;
};

/**
 * @brief Main application entry point and lifecycle manager.
 */
class Application {
public:
    /**
     * @brief Initializes SDL, window, renderer, and runtime modules.
     * @return True on success, false otherwise.
     */
    bool init();

    /**
     * @brief Executes the main loop until shutdown is requested.
     */
    void run();

    /**
     * @brief Releases all runtime resources.
     */
    void shutdown();

    /**
     * @brief Returns the most recent error message.
     * @return Error message set by a failed operation.
     */
    const std::string& last_error() const;

private:
    bool load_settings(const char* path);
    bool init_imgui();
    void shutdown_imgui();
    void build_docked_layout();
    bool render_vulkan_frame();

    bool running_ = false;
    bool sdl_initialized_ = false;
    bool imgui_initialized_ = false;

    app_settings settings_{};
    Window window_{};

    renderer::VulkanContext vulkan_context_{};
    renderer::RenderFrame render_frame_{};

    std::string last_error_{};
};

}  // namespace app
