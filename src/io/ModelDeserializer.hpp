#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/ExpectedCompat.hpp"
#include "model/FeatureTree.hpp"

namespace io {

/**
 * @brief Model deserialization error code.
 */
enum class ModelErrorCode {
    IoError,
    ParseError,
    SchemaVersionError,
    InvalidFormat,
    ParameterResolutionError,
    DependencyGraphError,
};

/**
 * @brief Per-feature deserialization issue.
 */
struct FeatureIssue {
    uint32_t feature_id = 0U;
    std::string feature_name{};
    std::string message{};
};

/**
 * @brief Model deserialization error payload.
 */
struct ModelError {
    ModelErrorCode code = ModelErrorCode::ParseError;
    std::string message{};
    std::vector<FeatureIssue> feature_issues{};
};

/**
 * @brief Deserializes model JSON into FeatureTree.
 */
class ModelDeserializer {
public:
    /**
     * @brief Parses model tree from JSON string.
     * @param json_text JSON payload text.
     * @return Parsed feature tree or model error.
     */
    [[nodiscard]] static std::expected<model::FeatureTree, ModelError> from_string(const std::string& json_text);

    /**
     * @brief Loads model tree from JSON file.
     * @param path Source file path.
     * @return Parsed feature tree or model error.
     */
    [[nodiscard]] static std::expected<model::FeatureTree, ModelError> from_file(const std::string& path);

    /**
     * @brief Resolves model parameter references (#name).
     * @param parameters Input parameters object.
     * @return Parameters object with resolved references.
     */
    [[nodiscard]] static std::expected<nlohmann::json, ModelError> resolve_parameters(const nlohmann::json& parameters);
};

}  // namespace io
