#include "renderer/RenderFrame.hpp"

namespace renderer {

bool RenderFrame::init(VkDevice device, uint32_t graphics_queue_family, uint32_t frames_in_flight) {
    if (device == VK_NULL_HANDLE || frames_in_flight == 0U) {
        return false;
    }

    device_ = device;
    frames_.clear();
    frames_.resize(frames_in_flight);

    for (frame_resources& frame : frames_) {
        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore image_available = VK_NULL_HANDLE;
        if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available) != VK_SUCCESS) {
            shutdown();
            return false;
        }
        frame.image_available = vk_wrap::semaphore(device_, image_available);

        VkSemaphore render_complete = VK_NULL_HANDLE;
        if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_complete) != VK_SUCCESS) {
            shutdown();
            return false;
        }
        frame.render_complete = vk_wrap::semaphore(device_, render_complete);

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkFence in_flight = VK_NULL_HANDLE;
        if (vkCreateFence(device_, &fence_info, nullptr, &in_flight) != VK_SUCCESS) {
            shutdown();
            return false;
        }
        frame.in_flight = vk_wrap::fence(device_, in_flight);

        VkCommandPoolCreateInfo command_pool_info{};
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_info.queueFamilyIndex = graphics_queue_family;

        VkCommandPool command_pool = VK_NULL_HANDLE;
        if (vkCreateCommandPool(device_, &command_pool_info, nullptr, &command_pool) != VK_SUCCESS) {
            shutdown();
            return false;
        }
        frame.command_pool = vk_wrap::command_pool(device_, command_pool);

        VkCommandBufferAllocateInfo command_allocate_info{};
        command_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_allocate_info.commandPool = frame.command_pool.get();
        command_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_allocate_info.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device_, &command_allocate_info, &frame.command_buffer) != VK_SUCCESS) {
            shutdown();
            return false;
        }
    }

    frame_index_ = 0;
    return true;
}

void RenderFrame::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        for (frame_resources& frame : frames_) {
            if (frame.command_pool.get() != VK_NULL_HANDLE && frame.command_buffer != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(device_, frame.command_pool.get(), 1, &frame.command_buffer);
                frame.command_buffer = VK_NULL_HANDLE;
            }
        }
    }

    frames_.clear();
    frame_index_ = 0;
    device_ = VK_NULL_HANDLE;
}

frame_resources& RenderFrame::current() {
    return frames_[frame_index_];
}

void RenderFrame::advance() {
    if (frames_.empty()) {
        return;
    }

    frame_index_ = (frame_index_ + 1U) % static_cast<uint32_t>(frames_.size());
}

uint32_t RenderFrame::frame_count() const {
    return static_cast<uint32_t>(frames_.size());
}

}  // namespace renderer
