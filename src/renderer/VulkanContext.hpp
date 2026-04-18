#pragma once

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "renderer/RenderFrame.hpp"
#include "renderer/VulkanHandles.hpp"

namespace renderer {

/**
 * @brief Vulkan bootstrap and per-frame presentation context.
 */
class VulkanContext {
public:
    /**
     * @brief Initializes Vulkan instance, device, queues, and swapchain.
     * @param window SDL window used for surface creation.
     * @param enable_vsync Enables FIFO present mode when true.
     * @return True on success, false otherwise.
     */
    bool init(SDL_Window* window, bool enable_vsync);

    /**
     * @brief Destroys Vulkan context resources.
     */
    void shutdown();

    /**
     * @brief Recreates swapchain after resize or presentation changes.
     * @param width New width.
     * @param height New height.
     * @param enable_vsync Enables FIFO present mode when true.
     * @return True on success, false otherwise.
     */
    bool recreate_swapchain(uint32_t width, uint32_t height, bool enable_vsync);

    /**
     * @brief Acquires image and starts command recording with dynamic rendering.
     * @param frame Active frame resource set.
     * @param out_image_index Acquired swapchain image index.
     * @return True when frame recording is ready, false if swapchain must be recreated.
     */
    bool begin_frame(frame_resources& frame, uint32_t* out_image_index);

    /**
     * @brief Ends dynamic rendering, submits work, and presents image.
     * @param frame Active frame resource set.
     * @param image_index Swapchain image index to present.
     * @return True on success, false on present failure or swapchain invalidation.
     */
    bool end_frame(frame_resources& frame, uint32_t image_index);

    /**
     * @brief Waits for all queued device operations to complete.
     */
    void wait_idle() const;

    /**
     * @brief Logical device accessor.
     * @return Vulkan device handle.
     */
    [[nodiscard]] VkDevice device() const;

    /**
     * @brief Vulkan instance accessor.
     * @return Vulkan instance handle.
     */
    [[nodiscard]] VkInstance instance() const;

    /**
     * @brief Physical device accessor.
     * @return Vulkan physical device handle.
     */
    [[nodiscard]] VkPhysicalDevice physical_device() const;

    /**
     * @brief Graphics queue accessor.
     * @return Vulkan queue handle.
     */
    [[nodiscard]] VkQueue graphics_queue() const;

    /**
     * @brief Graphics queue family index accessor.
     * @return Queue family index.
     */
    [[nodiscard]] uint32_t graphics_queue_family_index() const;

    /**
     * @brief Returns swapchain image count.
     * @return Number of swapchain images.
     */
    [[nodiscard]] uint32_t swapchain_image_count() const;

    /**
     * @brief Returns swapchain image format.
     * @return Swapchain image format.
     */
    [[nodiscard]] VkFormat swapchain_format() const;

    /**
     * @brief Stores the current camera matrices for viewport rendering.
     * @param view View matrix.
     * @param projection Projection matrix.
     * @param view_projection Combined view-projection matrix.
     */
    void set_camera_matrices(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& view_projection);

    /**
     * @brief Returns the last camera view matrix.
     * @return View matrix.
     */
    [[nodiscard]] const glm::mat4& view_matrix() const;

    /**
     * @brief Returns the last camera projection matrix.
     * @return Projection matrix.
     */
    [[nodiscard]] const glm::mat4& projection_matrix() const;

    /**
     * @brief Returns the last camera view-projection matrix.
     * @return View-projection matrix.
     */
    [[nodiscard]] const glm::mat4& view_projection_matrix() const;

private:
    bool create_instance();
    bool create_surface();
    bool pick_physical_device();
    bool create_device();
    bool create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();

    SDL_Window* window_ = nullptr;

    vk_wrap::instance instance_{};
    vk_wrap::surface surface_{};
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    vk_wrap::device device_{};

    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_index_ = UINT32_MAX;
    uint32_t present_queue_family_index_ = UINT32_MAX;

    vk_wrap::swapchain swapchain_{};
    std::vector<VkImage> swapchain_images_{};
    std::vector<vk_wrap::image_view> swapchain_image_views_{};
    std::vector<bool> image_initialized_{};

    VkFormat swapchain_format_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR swapchain_color_space_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D swapchain_extent_{0U, 0U};
    bool vsync_enabled_ = true;

    glm::mat4 camera_view_{};
    glm::mat4 camera_projection_{};
    glm::mat4 camera_view_projection_{};
};

}  // namespace renderer
