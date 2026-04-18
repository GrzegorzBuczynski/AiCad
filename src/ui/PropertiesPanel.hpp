#pragma once

#include "ui/Panel.hpp"

namespace ui {

/**
 * @brief Right dock panel with selection and parameter properties.
 */
class PropertiesPanel final : public Panel {
public:
    /**
     * @brief Draws the properties panel.
     */
    void draw() override;
};

}  // namespace ui
