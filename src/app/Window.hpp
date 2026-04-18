#pragma once

#include <cstdint>

#include <SDL3/SDL.h>

namespace app {

/**
 * @brief RAII wrapper around an SDL3 application window.
 */
class Window {
public:
    /**
     * @brief Creates a Vulkan-capable, resizable window.
     * @param title Window title.
     * @param width Initial width in pixels.
     * @param height Initial height in pixels.
     * @return True on success, false otherwise.
     */
    bool create(const char* title, uint32_t width, uint32_t height);

    /**
     * @brief Destroys the SDL window if it exists.
     */
    void destroy();

    /**
     * @brief Processes SDL events that are relevant to window state.
     * @param event SDL event to process.
     */
    void process_event(const SDL_Event& event);

    /**
     * @brief Returns true when the user has requested closing the window.
     * @return True when close was requested.
     */
    bool should_close() const;

    /**
     * @brief Reports whether a resize event has occurred.
     * @return True if a resize was detected.
     */
    bool is_resized() const;

    /**
     * @brief Clears the resize flag after swapchain recreation.
     */
    void clear_resize_flag();

    /**
     * @brief Returns the native SDL window handle.
     * @return Pointer to SDL_Window or nullptr.
     */
    SDL_Window* native_handle() const;

    /**
     * @brief Returns current drawable width.
     * @return Width in pixels.
     */
    uint32_t width() const;

    /**
     * @brief Returns current drawable height.
     * @return Height in pixels.
     */
    uint32_t height() const;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /**
     * @brief Constructs an empty window wrapper.
     */
    Window() = default;

    /**
     * @brief Ensures the window is destroyed on shutdown paths.
     */
    ~Window();

private:
    SDL_Window* window_ = nullptr;
    bool should_close_ = false;
    bool resized_ = false;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

}  // namespace app
