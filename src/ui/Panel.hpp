#pragma once

namespace ui {

/**
 * @brief Base interface for docked UI panels.
 */
class Panel {
public:
    /**
     * @brief Virtual destructor for polymorphic panel ownership.
     */
    virtual ~Panel() = default;

    /**
     * @brief Draws the panel contents.
     */
    virtual void draw() = 0;
};

}  // namespace ui
