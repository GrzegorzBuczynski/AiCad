#pragma once

#include <array>

#include <imgui.h>
#include <glm/glm.hpp>

#include "ui/Panel.hpp"

namespace ui {

/**
 * @brief Center dock panel hosting rendered viewport output.
 */
class ViewportPanel final : public Panel {
public:
    /**
     * @brief Sets render target texture to be displayed in viewport.
     * @param texture Vulkan-backed texture handle exposed as ImTextureID.
     */
    void set_texture(ImTextureID texture);

    /**
     * @brief Sets the camera matrices used by the viewport renderer.
     * @param view View matrix.
     * @param projection Projection matrix.
     * @param view_projection Combined view-projection matrix.
     */
    void set_camera_matrices(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& view_projection);

    /**
     * @brief Draws the viewport panel.
     */
    void draw() override;

private:
    ImTextureID viewport_texture_ = static_cast<ImTextureID>(0);
    glm::mat4 view_{};
    glm::mat4 projection_{};
    glm::mat4 view_projection_{};
};

}  // namespace ui
