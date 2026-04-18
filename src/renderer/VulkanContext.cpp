#include "renderer/VulkanContext.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <SDL3/SDL_vulkan.h>

namespace {

bool check_result(VkResult result) {
    return result == VK_SUCCESS;
}

}  // namespace

namespace renderer {

bool VulkanContext::init(SDL_Window* window, bool enable_vsync) {
    if (window == nullptr) {
        return false;
    }

    window_ = window;
    vsync_enabled_ = enable_vsync;

    if (!create_instance()) {
        return false;
    }
    if (!create_surface()) {
        return false;
    }
    if (!pick_physical_device()) {
        return false;
    }
    if (!create_device()) {
        return false;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window_, &width, &height);
    return create_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

void VulkanContext::shutdown() {
    if (device_.get() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_.get());
    }

    destroy_swapchain();
    device_.reset();
    surface_.reset();
    instance_.reset();

    physical_device_ = VK_NULL_HANDLE;
    window_ = nullptr;
    graphics_queue_ = VK_NULL_HANDLE;
    present_queue_ = VK_NULL_HANDLE;
    graphics_queue_family_index_ = UINT32_MAX;
    present_queue_family_index_ = UINT32_MAX;
}

bool VulkanContext::recreate_swapchain(uint32_t width, uint32_t height, bool enable_vsync) {
    if (device_.get() == VK_NULL_HANDLE || width == 0U || height == 0U) {
        return false;
    }

    vkDeviceWaitIdle(device_.get());
    vsync_enabled_ = enable_vsync;

    destroy_swapchain();
    return create_swapchain(width, height);
}

bool VulkanContext::begin_frame(frame_resources& frame, uint32_t* out_image_index) {
    if (out_image_index == nullptr) {
        return false;
    }

    if (!check_result(vkWaitForFences(device_.get(), 1, &frame.in_flight.handle, VK_TRUE, UINT64_MAX))) {
        return false;
    }

    if (!check_result(vkResetFences(device_.get(), 1, &frame.in_flight.handle))) {
        return false;
    }

    VkResult acquire_result = vkAcquireNextImageKHR(
        device_.get(),
        swapchain_.get(),
        UINT64_MAX,
        frame.image_available.get(),
        VK_NULL_HANDLE,
        out_image_index);

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        return false;
    }
    if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    if (!check_result(vkResetCommandPool(device_.get(), frame.command_pool.get(), 0))) {
        return false;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!check_result(vkBeginCommandBuffer(frame.command_buffer, &begin_info))) {
        return false;
    }

    VkImageMemoryBarrier to_color{};
    to_color.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_color.srcAccessMask = 0;
    to_color.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    to_color.oldLayout = image_initialized_[*out_image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
    to_color.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_color.image = swapchain_images_[*out_image_index];
    to_color.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_color.subresourceRange.baseMipLevel = 0;
    to_color.subresourceRange.levelCount = 1;
    to_color.subresourceRange.baseArrayLayer = 0;
    to_color.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        frame.command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &to_color);

    VkClearValue clear_value{};
    clear_value.color.float32[0] = 0.08F;
    clear_value.color.float32[1] = 0.10F;
    clear_value.color.float32[2] = 0.13F;
    clear_value.color.float32[3] = 1.00F;

    VkRenderingAttachmentInfo color_attachment{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = swapchain_image_views_[*out_image_index].get();
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue = clear_value;

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset = {0, 0};
    rendering_info.renderArea.extent = swapchain_extent_;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    vkCmdBeginRendering(frame.command_buffer, &rendering_info);
    return true;
}

bool VulkanContext::end_frame(frame_resources& frame, uint32_t image_index) {
    vkCmdEndRendering(frame.command_buffer);

    VkImageMemoryBarrier to_present{};
    to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    to_present.dstAccessMask = 0;
    to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.image = swapchain_images_[image_index];
    to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_present.subresourceRange.baseMipLevel = 0;
    to_present.subresourceRange.levelCount = 1;
    to_present.subresourceRange.baseArrayLayer = 0;
    to_present.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        frame.command_buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &to_present);

    if (!check_result(vkEndCommandBuffer(frame.command_buffer))) {
        return false;
    }

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame.image_available.handle;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &frame.render_complete.handle;

    if (!check_result(vkQueueSubmit(graphics_queue_, 1, &submit_info, frame.in_flight.handle))) {
        return false;
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &frame.render_complete.handle;
    present_info.swapchainCount = 1;
    const VkSwapchainKHR swapchain_handle = swapchain_.get();
    present_info.pSwapchains = &swapchain_handle;
    present_info.pImageIndices = &image_index;

    const VkResult present_result = vkQueuePresentKHR(present_queue_, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
        return false;
    }

    image_initialized_[image_index] = true;
    return present_result == VK_SUCCESS;
}

void VulkanContext::wait_idle() const {
    if (device_.get() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_.get());
    }
}

VkDevice VulkanContext::device() const {
    return device_.get();
}

uint32_t VulkanContext::graphics_queue_family_index() const {
    return graphics_queue_family_index_;
}

bool VulkanContext::create_instance() {
    uint32_t extension_count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (extensions == nullptr || extension_count == 0U) {
        return false;
    }

    std::vector<const char*> extension_names;
    extension_names.reserve(extension_count + 1U);
    for (uint32_t i = 0; i < extension_count; ++i) {
        extension_names.push_back(extensions[i]);
    }

#if defined(DEBUG)
    const std::array<const char*, 1> layers = {"VK_LAYER_KHRONOS_validation"};
#endif

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "VulcanCAD";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "VulcanCAD";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extension_names.size());
    create_info.ppEnabledExtensionNames = extension_names.data();
#if defined(DEBUG)
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();
#endif

    VkInstance instance = VK_NULL_HANDLE;
    if (!check_result(vkCreateInstance(&create_info, nullptr, &instance))) {
        return false;
    }

    instance_ = vk_wrap::instance(instance);
    return true;
}

bool VulkanContext::create_surface() {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window_, instance_.get(), nullptr, &surface)) {
        return false;
    }

    surface_ = vk_wrap::surface(instance_.get(), surface);
    return true;
}

bool VulkanContext::pick_physical_device() {
    uint32_t device_count = 0;
    if (!check_result(vkEnumeratePhysicalDevices(instance_.get(), &device_count, nullptr)) || device_count == 0U) {
        return false;
    }

    std::vector<VkPhysicalDevice> candidates(device_count);
    if (!check_result(vkEnumeratePhysicalDevices(instance_.get(), &device_count, candidates.data()))) {
        return false;
    }

    for (VkPhysicalDevice candidate : candidates) {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, nullptr);
        if (queue_family_count == 0U) {
            continue;
        }

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, queue_families.data());

        uint32_t graphics_index = UINT32_MAX;
        uint32_t present_index = UINT32_MAX;

        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
                graphics_index = i;
            }

            VkBool32 supports_present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_.get(), &supports_present);
            if (supports_present == VK_TRUE) {
                present_index = i;
            }
        }

        if (graphics_index != UINT32_MAX && present_index != UINT32_MAX) {
            physical_device_ = candidate;
            graphics_queue_family_index_ = graphics_index;
            present_queue_family_index_ = present_index;
            return true;
        }
    }

    return false;
}

bool VulkanContext::create_device() {
    constexpr float queue_priority = 1.0F;

    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    queue_infos.reserve(2);

    VkDeviceQueueCreateInfo graphics_queue_info{};
    graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_info.queueFamilyIndex = graphics_queue_family_index_;
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = &queue_priority;
    queue_infos.push_back(graphics_queue_info);

    if (present_queue_family_index_ != graphics_queue_family_index_) {
        VkDeviceQueueCreateInfo present_queue_info{};
        present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        present_queue_info.queueFamilyIndex = present_queue_family_index_;
        present_queue_info.queueCount = 1;
        present_queue_info.pQueuePriorities = &queue_priority;
        queue_infos.push_back(present_queue_info);
    }

    std::array<const char*, 1> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{};
    dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic_rendering_features.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &dynamic_rendering_features;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    VkDevice device = VK_NULL_HANDLE;
    if (!check_result(vkCreateDevice(physical_device_, &create_info, nullptr, &device))) {
        return false;
    }

    device_ = vk_wrap::device(device);
    vkGetDeviceQueue(device_.get(), graphics_queue_family_index_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_.get(), present_queue_family_index_, 0, &present_queue_);
    return true;
}

bool VulkanContext::create_swapchain(uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR capabilities{};
    if (!check_result(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_.get(), &capabilities))) {
        return false;
    }

    uint32_t format_count = 0;
    if (!check_result(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_.get(), &format_count, nullptr)) ||
        format_count == 0U) {
        return false;
    }

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    if (!check_result(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_.get(), &format_count, formats.data()))) {
        return false;
    }

    VkSurfaceFormatKHR chosen_format = formats[0];
    for (const VkSurfaceFormatKHR& candidate : formats) {
        if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
            candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = candidate;
            break;
        }
    }

    uint32_t present_mode_count = 0;
    if (!check_result(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_.get(), &present_mode_count, nullptr)) ||
        present_mode_count == 0U) {
        return false;
    }

    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    if (!check_result(vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device_,
            surface_.get(),
            &present_mode_count,
            present_modes.data()))) {
        return false;
    }

    VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (!vsync_enabled_) {
        for (VkPresentModeKHR mode : present_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                chosen_present_mode = mode;
                break;
            }
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                chosen_present_mode = mode;
            }
        }
    }

    const uint32_t clamped_width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    const uint32_t clamped_height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    VkExtent2D extent{};
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = clamped_width;
        extent.height = clamped_height;
    }

    uint32_t image_count = capabilities.minImageCount + 1U;
    if (capabilities.maxImageCount > 0U && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface_.get();
    create_info.minImageCount = image_count;
    create_info.imageFormat = chosen_format.format;
    create_info.imageColorSpace = chosen_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const uint32_t queue_family_indices[2] = {graphics_queue_family_index_, present_queue_family_index_};
    if (graphics_queue_family_index_ != present_queue_family_index_) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = chosen_present_mode;
    create_info.clipped = VK_TRUE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    if (!check_result(vkCreateSwapchainKHR(device_.get(), &create_info, nullptr, &swapchain))) {
        return false;
    }

    swapchain_ = vk_wrap::swapchain(device_.get(), swapchain);
    swapchain_format_ = chosen_format.format;
    swapchain_color_space_ = chosen_format.colorSpace;
    swapchain_extent_ = extent;

    uint32_t swapchain_image_count = 0;
    if (!check_result(vkGetSwapchainImagesKHR(device_.get(), swapchain_.get(), &swapchain_image_count, nullptr)) ||
        swapchain_image_count == 0U) {
        return false;
    }

    swapchain_images_.resize(swapchain_image_count);
    if (!check_result(vkGetSwapchainImagesKHR(
            device_.get(),
            swapchain_.get(),
            &swapchain_image_count,
            swapchain_images_.data()))) {
        return false;
    }

    swapchain_image_views_.clear();
    swapchain_image_views_.reserve(swapchain_images_.size());

    for (VkImage image : swapchain_images_) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain_format_;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkImageView image_view = VK_NULL_HANDLE;
        if (!check_result(vkCreateImageView(device_.get(), &view_info, nullptr, &image_view))) {
            destroy_swapchain();
            return false;
        }

        swapchain_image_views_.emplace_back(device_.get(), image_view);
    }

    image_initialized_.assign(swapchain_images_.size(), false);
    return true;
}

void VulkanContext::destroy_swapchain() {
    swapchain_image_views_.clear();
    swapchain_images_.clear();
    image_initialized_.clear();
    swapchain_.reset();
}

}  // namespace renderer
