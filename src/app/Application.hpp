#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "app/Window.hpp"
#include "renderer/RenderFrame.hpp"
#include "renderer/VulkanContext.hpp"
#include "model/FeatureTree.hpp"
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
    void process_feature_tree_actions();
    bool execute_feature_rebuild(const model::RebuildRequest& request, const std::string& payload_override = "", bool allow_retry_popup = true);
    void draw_worker_retry_popup();

    struct WorkerRetryContext {
        model::RebuildRequest request{};
        uint32_t failed_node_id = 0U;
        std::array<char, 2048> payload_buffer{};
    };

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
    model::FeatureTree feature_tree_{};
    scene::Camera camera_{};
    scene::State saved_camera_state_{};
    sketch::SketchDocument sketch_document_{glm::vec3{0.0f, 0.0f, 0.0f}};
    sketch::SketchView sketch_view_{};

    vk_wrap::descriptor_pool imgui_descriptor_pool_{};
    std::array<VkFormat, 1> imgui_color_attachment_formats_{};
    VkPipelineRenderingCreateInfoKHR imgui_pipeline_rendering_info_{};

    bool dock_layout_built_ = false;
    bool imgui_backend_initialized_ = false;
    bool camera_session_loaded_ = false;
    bool worker_retry_popup_opened_ = false;
    std::optional<WorkerRetryContext> worker_retry_context_{};

    std::string last_error_{};
};

}  // namespace app
