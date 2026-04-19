#include "io/ModelSerializer.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

namespace io {

namespace {

constexpr const char* k_schema_version = "1.0";

using ordered_json = nlohmann::ordered_json;

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
    std::vector<ordered_json>& output) {
    if (feature == nullptr) {
        return;
    }

    ordered_json dependencies = ordered_json::array();
    if (feature->parent != nullptr) {
        dependencies.push_back(feature->parent->id);
    }

    ordered_json feature_json = ordered_json::object();
    feature_json["id"] = feature->id;
    feature_json["expanded"] = feature->expanded;
    feature_json["type"] = to_string(feature->type);
    feature_json["name"] = feature->name;
    feature_json["state"] = to_string(feature->state);
    feature_json["suppressed"] = feature->suppressed;
    feature_json["parent_id"] = feature->parent != nullptr ? ordered_json(feature->parent->id) : ordered_json(nullptr);
    feature_json["dependencies"] = dependencies;

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

nlohmann::ordered_json ModelSerializer::to_json(const model::FeatureTree& tree, const ModelSerializerOptions& options) {
    std::vector<ordered_json> features{};
    collect_features(tree.root(), options, features);

    ordered_json metadata = options.metadata;
    if (!metadata.is_object()) {
        metadata = ordered_json::object();
    }
    metadata["generator"] = "VulcanCAD";

    ordered_json parameters = options.parameters;
    if (!parameters.is_object()) {
        parameters = ordered_json::object();
    }

    ordered_json session = options.session;
    if (!session.is_object()) {
        session = ordered_json::object();
    }

    ordered_json root = ordered_json::object();
    root["schema_version"] = k_schema_version;
    root["name"] = options.model_name;
    root["units"] = options.units;
    root["parameters"] = parameters;
    root["features"] = features;
    root["metadata"] = metadata;
    root["session"] = session;
    return root;
}

std::string ModelSerializer::to_string(const model::FeatureTree& tree, const ModelSerializerOptions& options) {
    const ordered_json payload = to_json(tree, options);
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
