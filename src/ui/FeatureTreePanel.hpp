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

private:
    std::vector<std::string> placeholder_features_{};
};

}  // namespace ui
