#include "io/CameraSession.hpp"

#include <filesystem>
#include <fstream>

namespace io {

bool CameraSession::load(const std::string& path, scene::Camera& camera) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::string data;
    file.seekg(0, std::ios::end);
    const std::streampos size = file.tellg();
    if (size <= 0) {
        return false;
    }

    data.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (!file.good() && !file.eof()) {
        return false;
    }

    const nlohmann::json json_state = nlohmann::json::parse(data, nullptr, false);
    if (json_state.is_discarded()) {
        return false;
    }

    return camera.from_json(json_state);
}

bool CameraSession::save(const std::string& path, const scene::Camera& camera) {
    const std::filesystem::path session_path(path);
    std::error_code error_code;
    if (session_path.has_parent_path()) {
        std::filesystem::create_directories(session_path.parent_path(), error_code);
    }

    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    const nlohmann::json json_state = camera.to_json();
    file << json_state.dump(2);
    return file.good();
}

}  // namespace io
