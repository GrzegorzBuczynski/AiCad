#pragma once

#include <array>
#include <optional>
#include <vector>

#include <imgui.h>
#include <glm/glm.hpp>

#include "geometry/IGeometryKernel.hpp"
#include "model/Plane.hpp"
#include "sketch/GridFeature.hpp"
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

    /**
     * @brief Returns viewport content origin in screen coordinates.
     * @return Screen-space origin.
     */
    [[nodiscard]] ImVec2 content_origin() const;

    /**
     * @brief Returns viewport content size in pixels.
     * @return Pixel size.
     */
    [[nodiscard]] ImVec2 content_size() const;

    /**
     * @brief Returns whether viewport was hovered during last draw.
     * @return True when hovered.
     */
    [[nodiscard]] bool is_hovered() const;

    /**
     * @brief Sets persistent sketch profiles to be shown in 3D viewport.
     * @param profiles Closed sketch profiles.
     */
    void set_sketch_profiles(const std::vector<geometry::Profile>& profiles);

    /**
     * @brief Sets sketch plane used to display sketch entities in 3D viewport.
     * @param plane Active sketch plane.
     */
    void set_sketch_plane(const model::Plane& plane);

    /**
     * @brief Sets optional grid feature visualization data.
     * @param feature Grid feature to render, or nullopt when absent.
     */
    void set_grid_feature(std::optional<sketch::GridFeature> feature);

    /**
     * @brief Sets viewport-local font scale.
     * @param scale Requested font scale.
     */
    void set_font_scale(float scale);

private:
    ImTextureID viewport_texture_ = static_cast<ImTextureID>(0);
    glm::mat4 view_{};
    glm::mat4 projection_{};
    glm::mat4 view_projection_{};
    std::vector<geometry::Profile> sketch_profiles_{};
    model::Plane sketch_plane_{glm::vec3{0.0f, 0.0f, 0.0f}};
    std::optional<sketch::GridFeature> grid_feature_{};
    ImVec2 content_origin_{0.0f, 0.0f};
    ImVec2 content_size_{0.0f, 0.0f};
    bool hovered_ = false;
    float font_scale_ = 1.0f;
};

}  // namespace ui
