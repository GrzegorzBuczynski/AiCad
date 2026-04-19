#pragma once

#include <string>
#include <vector>

#include "ui/Panel.hpp"

namespace ui {

/**
 * @brief Left dock panel showing feature hierarchy.
 */
class FeatureTreePanel final : public Panel {
public:
    /**
     * @brief Draws the feature tree panel.
     */
    void draw() override;

    /**
     * @brief Consumes pending sketch-open request emitted by double click.
     * @return True when a sketch node requested opening sketch mode.
     */
    bool consume_open_sketch_request();

    /**
     * @brief Consumes pending plane-properties request emitted by double click.
     * @return True when a plane node requested opening plane properties.
     */
    bool consume_open_plane_properties_request();

    /**
     * @brief Sets panel-local font scale.
     * @param scale Requested scale factor.
     */
    void set_font_scale(float scale);

private:
    std::vector<std::string> placeholder_features_{};
    bool open_sketch_request_ = false;
    bool open_plane_properties_request_ = false;
    float font_scale_ = 1.0f;
};

}  // namespace ui
