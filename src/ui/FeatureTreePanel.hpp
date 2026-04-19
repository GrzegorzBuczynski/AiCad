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

private:
    std::vector<std::string> placeholder_features_{};
    bool open_sketch_request_ = false;
};

}  // namespace ui
