#pragma once

#include <cstdint>
#include <array>
#include <string>

#include "app/Window.hpp"
#include "renderer/RenderFrame.hpp"
#include "renderer/VulkanContext.hpp"
#include "scene/Camera.hpp"
#include "sketch/SketchDocument.hpp"
#include "sketch/SketchView.hpp"
#include "io/CameraSession.hpp"
#include "ui/FeatureTreePanel.hpp"
#include "ui/PropertiesPanel.hpp"
#include "ui/ViewportPanel.hpp"

#include <vulkan/vulkan.h>

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
    void draw_menu_bar();
    void draw_status_bar();
    void build_initial_dock_layout();
    bool init_imgui_backend();
    bool render_vulkan_frame();
    void sync_camera_to_viewport();
    void persist_camera_session() const;

    bool running_ = false;
    bool sdl_initialized_ = false;
    bool imgui_initialized_ = false;

    app_settings settings_{};
    Window window_{};

    renderer::VulkanContext vulkan_context_{};
    renderer::RenderFrame render_frame_{};

    ui::FeatureTreePanel feature_tree_panel_{};
    ui::ViewportPanel viewport_panel_{};
    ui::PropertiesPanel properties_panel_{};
    scene::Camera camera_{};
    scene::State saved_camera_state_{};
    sketch::SketchDocument sketch_document_{};
    sketch::SketchView sketch_view_{};

    vk_wrap::descriptor_pool imgui_descriptor_pool_{};
    std::array<VkFormat, 1> imgui_color_attachment_formats_{};
    VkPipelineRenderingCreateInfoKHR imgui_pipeline_rendering_info_{};

    bool dock_layout_built_ = false;
    bool imgui_backend_initialized_ = false;
    bool camera_session_loaded_ = false;

    std::string last_error_{};
};

}  // namespace app
