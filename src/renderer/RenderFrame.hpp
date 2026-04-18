#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include "renderer/VulkanHandles.hpp"

namespace renderer {

/**
 * @brief Per-frame synchronization resources and command recording state.
 */
struct frame_resources {
    vk_wrap::semaphore image_available{};
    vk_wrap::semaphore render_complete{};
    vk_wrap::fence in_flight{};
    vk_wrap::command_pool command_pool{};
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
};

/**
 * @brief Owns and rotates frame-in-flight synchronization objects.
 */
class RenderFrame {
public:
    /**
     * @brief Creates per-frame semaphores, fences, and command buffers.
     * @param device Logical Vulkan device.
     * @param graphics_queue_family Graphics queue family index.
     * @param frames_in_flight Number of frames to allocate.
     * @return True on success, false otherwise.
     */
    bool init(VkDevice device, uint32_t graphics_queue_family, uint32_t frames_in_flight);

    /**
     * @brief Releases all per-frame resources.
     */
    void shutdown();

    /**
     * @brief Returns the active frame resource set.
     * @return Mutable reference to active frame resources.
     */
    frame_resources& current();

    /**
     * @brief Advances to the next frame-in-flight.
     */
    void advance();

    /**
     * @brief Number of allocated frame resource sets.
     * @return Frame count.
     */
    [[nodiscard]] uint32_t frame_count() const;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    std::vector<frame_resources> frames_{};
    uint32_t frame_index_ = 0;
};

}  // namespace renderer
