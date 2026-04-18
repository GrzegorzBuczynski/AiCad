#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "scene/Camera.hpp"

namespace io {

/**
 * @brief Loads and saves camera state from a JSON session file.
 */
class CameraSession {
public:
    /**
     * @brief Loads camera state from disk.
     * @param path Session file path.
     * @param camera Camera to restore.
     * @return True on success, false otherwise.
     */
    static bool load(const std::string& path, scene::Camera& camera);

    /**
     * @brief Saves camera state to disk.
     * @param path Session file path.
     * @param camera Camera to persist.
     * @return True on success, false otherwise.
     */
    static bool save(const std::string& path, const scene::Camera& camera);
};

}  // namespace io
