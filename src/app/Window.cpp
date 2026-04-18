#include "app/Window.hpp"

#include <utility>

namespace app {

Window::~Window() {
    destroy();
}

bool Window::create(const char* title, uint32_t width, uint32_t height) {
    const SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    window_ = SDL_CreateWindow(title, static_cast<int>(width), static_cast<int>(height), flags);
    if (window_ == nullptr) {
        return false;
    }

    width_ = width;
    height_ = height;
    should_close_ = false;
    resized_ = false;
    return true;
}

void Window::destroy() {
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    should_close_ = false;
    resized_ = false;
    width_ = 0;
    height_ = 0;
}

void Window::process_event(const SDL_Event& event) {
    if (event.type == SDL_EVENT_QUIT) {
        should_close_ = true;
        return;
    }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        should_close_ = true;
        return;
    }

    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
        width_ = static_cast<uint32_t>(event.window.data1);
        height_ = static_cast<uint32_t>(event.window.data2);
        resized_ = true;
    }
}

bool Window::should_close() const {
    return should_close_;
}

bool Window::is_resized() const {
    return resized_;
}

void Window::clear_resize_flag() {
    resized_ = false;
}

SDL_Window* Window::native_handle() const {
    return window_;
}

uint32_t Window::width() const {
    return width_;
}

uint32_t Window::height() const {
    return height_;
}

}  // namespace app
