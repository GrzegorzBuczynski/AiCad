#pragma once

#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "model/FeatureTree.hpp"

namespace io {

/**
 * @brief Serialization settings for model JSON export.
 */
struct ModelSerializerOptions {
    bool pretty_print = true;
    std::string model_name = "VulcanCAD Model";
    std::string units = "mm";
    nlohmann::ordered_json parameters = nlohmann::ordered_json::object();
    nlohmann::ordered_json metadata = nlohmann::ordered_json::object();
    nlohmann::ordered_json session = nlohmann::ordered_json::object();
    std::unordered_map<uint32_t, nlohmann::ordered_json> feature_payloads{};
    nlohmann::ordered_json extra_features = nlohmann::ordered_json::array();
};

/**
 * @brief Serializes feature tree model into JSON payload.
 */
class ModelSerializer {
public:
    /**
     * @brief Converts feature tree into JSON object.
     * @param tree Source feature tree.
     * @param options Serialization settings.
     * @return JSON model payload.
     */
    [[nodiscard]] static nlohmann::ordered_json to_json(const model::FeatureTree& tree, const ModelSerializerOptions& options = {});

    /**
     * @brief Converts feature tree into string JSON.
     * @param tree Source feature tree.
     * @param options Serialization settings.
     * @return Serialized JSON string.
     */
    [[nodiscard]] static std::string to_string(const model::FeatureTree& tree, const ModelSerializerOptions& options = {});

    /**
     * @brief Writes serialized model JSON to file.
     * @param path Destination file path.
     * @param tree Source feature tree.
     * @param options Serialization settings.
     * @return True when file was written successfully.
     */
    [[nodiscard]] static bool save_to_file(const std::string& path, const model::FeatureTree& tree, const ModelSerializerOptions& options = {});
};

}  // namespace io
