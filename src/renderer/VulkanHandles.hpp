#pragma once

#include <utility>

#include <vulkan/vulkan.h>

namespace vk_wrap {

struct instance {
    VkInstance handle = VK_NULL_HANDLE;

    instance() = default;
    explicit instance(VkInstance value) : handle(value) {}

    instance(const instance&) = delete;
    instance& operator=(const instance&) = delete;

    instance(instance&& other) noexcept : handle(std::exchange(other.handle, VK_NULL_HANDLE)) {}

    instance& operator=(instance&& other) noexcept {
        if (this != &other) {
            reset();
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~instance() { reset(); }

    void reset() {
        if (handle != VK_NULL_HANDLE) {
            vkDestroyInstance(handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkInstance get() const { return handle; }
};

struct surface {
    VkInstance owner = VK_NULL_HANDLE;
    VkSurfaceKHR handle = VK_NULL_HANDLE;

    surface() = default;
    surface(VkInstance instance, VkSurfaceKHR value) : owner(instance), handle(value) {}

    surface(const surface&) = delete;
    surface& operator=(const surface&) = delete;

    surface(surface&& other) noexcept
        : owner(std::exchange(other.owner, VK_NULL_HANDLE)),
          handle(std::exchange(other.handle, VK_NULL_HANDLE)) {}

    surface& operator=(surface&& other) noexcept {
        if (this != &other) {
            reset();
            owner = std::exchange(other.owner, VK_NULL_HANDLE);
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~surface() { reset(); }

    void reset() {
        if (owner != VK_NULL_HANDLE && handle != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(owner, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkSurfaceKHR get() const { return handle; }
};

struct device {
    VkDevice handle = VK_NULL_HANDLE;

    device() = default;
    explicit device(VkDevice value) : handle(value) {}

    device(const device&) = delete;
    device& operator=(const device&) = delete;

    device(device&& other) noexcept : handle(std::exchange(other.handle, VK_NULL_HANDLE)) {}

    device& operator=(device&& other) noexcept {
        if (this != &other) {
            reset();
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~device() { reset(); }

    void reset() {
        if (handle != VK_NULL_HANDLE) {
            vkDestroyDevice(handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkDevice get() const { return handle; }
};

struct swapchain {
    VkDevice owner = VK_NULL_HANDLE;
    VkSwapchainKHR handle = VK_NULL_HANDLE;

    swapchain() = default;
    swapchain(VkDevice device, VkSwapchainKHR value) : owner(device), handle(value) {}

    swapchain(const swapchain&) = delete;
    swapchain& operator=(const swapchain&) = delete;

    swapchain(swapchain&& other) noexcept
        : owner(std::exchange(other.owner, VK_NULL_HANDLE)),
          handle(std::exchange(other.handle, VK_NULL_HANDLE)) {}

    swapchain& operator=(swapchain&& other) noexcept {
        if (this != &other) {
            reset();
            owner = std::exchange(other.owner, VK_NULL_HANDLE);
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~swapchain() { reset(); }

    void reset() {
        if (owner != VK_NULL_HANDLE && handle != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(owner, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkSwapchainKHR get() const { return handle; }
};

struct image_view {
    VkDevice owner = VK_NULL_HANDLE;
    VkImageView handle = VK_NULL_HANDLE;

    image_view() = default;
    image_view(VkDevice device, VkImageView value) : owner(device), handle(value) {}

    image_view(const image_view&) = delete;
    image_view& operator=(const image_view&) = delete;

    image_view(image_view&& other) noexcept
        : owner(std::exchange(other.owner, VK_NULL_HANDLE)),
          handle(std::exchange(other.handle, VK_NULL_HANDLE)) {}

    image_view& operator=(image_view&& other) noexcept {
        if (this != &other) {
            reset();
            owner = std::exchange(other.owner, VK_NULL_HANDLE);
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~image_view() { reset(); }

    void reset() {
        if (owner != VK_NULL_HANDLE && handle != VK_NULL_HANDLE) {
            vkDestroyImageView(owner, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkImageView get() const { return handle; }
};

struct semaphore {
    VkDevice owner = VK_NULL_HANDLE;
    VkSemaphore handle = VK_NULL_HANDLE;

    semaphore() = default;
    semaphore(VkDevice device, VkSemaphore value) : owner(device), handle(value) {}

    semaphore(const semaphore&) = delete;
    semaphore& operator=(const semaphore&) = delete;

    semaphore(semaphore&& other) noexcept
        : owner(std::exchange(other.owner, VK_NULL_HANDLE)),
          handle(std::exchange(other.handle, VK_NULL_HANDLE)) {}

    semaphore& operator=(semaphore&& other) noexcept {
        if (this != &other) {
            reset();
            owner = std::exchange(other.owner, VK_NULL_HANDLE);
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~semaphore() { reset(); }

    void reset() {
        if (owner != VK_NULL_HANDLE && handle != VK_NULL_HANDLE) {
            vkDestroySemaphore(owner, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkSemaphore get() const { return handle; }
};

struct fence {
    VkDevice owner = VK_NULL_HANDLE;
    VkFence handle = VK_NULL_HANDLE;

    fence() = default;
    fence(VkDevice device, VkFence value) : owner(device), handle(value) {}

    fence(const fence&) = delete;
    fence& operator=(const fence&) = delete;

    fence(fence&& other) noexcept
        : owner(std::exchange(other.owner, VK_NULL_HANDLE)),
          handle(std::exchange(other.handle, VK_NULL_HANDLE)) {}

    fence& operator=(fence&& other) noexcept {
        if (this != &other) {
            reset();
            owner = std::exchange(other.owner, VK_NULL_HANDLE);
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~fence() { reset(); }

    void reset() {
        if (owner != VK_NULL_HANDLE && handle != VK_NULL_HANDLE) {
            vkDestroyFence(owner, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkFence get() const { return handle; }
};

struct command_pool {
    VkDevice owner = VK_NULL_HANDLE;
    VkCommandPool handle = VK_NULL_HANDLE;

    command_pool() = default;
    command_pool(VkDevice device, VkCommandPool value) : owner(device), handle(value) {}

    command_pool(const command_pool&) = delete;
    command_pool& operator=(const command_pool&) = delete;

    command_pool(command_pool&& other) noexcept
        : owner(std::exchange(other.owner, VK_NULL_HANDLE)),
          handle(std::exchange(other.handle, VK_NULL_HANDLE)) {}

    command_pool& operator=(command_pool&& other) noexcept {
        if (this != &other) {
            reset();
            owner = std::exchange(other.owner, VK_NULL_HANDLE);
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~command_pool() { reset(); }

    void reset() {
        if (owner != VK_NULL_HANDLE && handle != VK_NULL_HANDLE) {
            vkDestroyCommandPool(owner, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkCommandPool get() const { return handle; }
};

}  // namespace vk_wrap
