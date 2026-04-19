#include "io/ModelSerializer.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

namespace io {

namespace {

constexpr const char* k_schema_version = "1.0";

const char* to_string(model::FeatureType type) {
    switch (type) {
    case model::FeatureType::PartContainer:
        return "PartContainer";
    case model::FeatureType::SketchFeature:
        return "SketchFeature";
    case model::FeatureType::ExtrudeFeature:
        return "ExtrudeFeature";
    case model::FeatureType::RevolveFeature:
        return "RevolveFeature";
    case model::FeatureType::FilletFeature:
        return "FilletFeature";
    case model::FeatureType::ChamferFeature:
        return "ChamferFeature";
    case model::FeatureType::ShellFeature:
        return "ShellFeature";
    case model::FeatureType::HoleFeature:
        return "HoleFeature";
    case model::FeatureType::MirrorFeature:
        return "MirrorFeature";
    }

    return "SketchFeature";
}

const char* to_string(model::FeatureState state) {
    switch (state) {
    case model::FeatureState::Valid:
        return "Valid";
    case model::FeatureState::Warning:
        return "Warning";
    case model::FeatureState::Error:
        return "Error";
    case model::FeatureState::Suppressed:
        return "Suppressed";
    }

    return "Valid";
}

void collect_features(
    const model::FeatureNode* feature,
    const ModelSerializerOptions& options,
    std::vector<nlohmann::json>& output) {
    if (feature == nullptr) {
        return;
    }

    nlohmann::json dependencies = nlohmann::json::array();
    if (feature->parent != nullptr) {
        dependencies.push_back(feature->parent->id);
    }

    nlohmann::json feature_json = {
        {"id", feature->id},
        {"type", to_string(feature->type)},
        {"name", feature->name},
        {"state", to_string(feature->state)},
        {"suppressed", feature->suppressed},
        {"expanded", feature->expanded},
        {"parent_id", feature->parent != nullptr ? nlohmann::json(feature->parent->id) : nlohmann::json(nullptr)},
        {"dependencies", dependencies},
    };

    const auto payload_it = options.feature_payloads.find(feature->id);
    if (payload_it != options.feature_payloads.end()) {
        feature_json["payload"] = payload_it->second;
        if (feature->type == model::FeatureType::ExtrudeFeature && payload_it->second.contains("profile_id")) {
            feature_json["profile_id"] = payload_it->second["profile_id"];
        }
    } else if (feature->type == model::FeatureType::ExtrudeFeature && feature->parent != nullptr) {
        feature_json["profile_id"] = feature->parent->id;
    }

    output.push_back(std::move(feature_json));

    for (const model::FeatureNode* child : feature->children) {
        collect_features(child, options, output);
    }
}

}  // namespace

nlohmann::json ModelSerializer::to_json(const model::FeatureTree& tree, const ModelSerializerOptions& options) {
    std::vector<nlohmann::json> features{};
    collect_features(tree.root(), options, features);

    nlohmann::json metadata = options.metadata;
    if (!metadata.is_object()) {
        metadata = nlohmann::json::object();
    }
    metadata["generator"] = "VulcanCAD";

    nlohmann::json parameters = options.parameters;
    if (!parameters.is_object()) {
        parameters = nlohmann::json::object();
    }

    nlohmann::json session = options.session;
    if (!session.is_object()) {
        session = nlohmann::json::object();
    }

    return {
        {"schema_version", k_schema_version},
        {"name", options.model_name},
        {"units", options.units},
        {"parameters", parameters},
        {"features", features},
        {"metadata", metadata},
        {"session", session},
    };
}

std::string ModelSerializer::to_string(const model::FeatureTree& tree, const ModelSerializerOptions& options) {
    const nlohmann::json payload = to_json(tree, options);
    return payload.dump(options.pretty_print ? 2 : -1);
}

bool ModelSerializer::save_to_file(const std::string& path, const model::FeatureTree& tree, const ModelSerializerOptions& options) {
    const std::filesystem::path file_path(path);
    std::error_code error_code;
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path(), error_code);
    }

    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << to_string(tree, options);
    return output.good();
}

}  // namespace io
