#include "io/ModelSerializer.hpp"

#include <algorithm>
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
    case model::FeatureType::Point:
        return "Point";
    case model::FeatureType::Line:
        return "Line";
    case model::FeatureType::Plane:
        return "Plane";
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

    ordered_json dependencies = ordered_json::object();
    if (!feature->children.empty()) {
        dependencies["id"] = feature->children.front()->id;
    }

    ordered_json feature_json = ordered_json::object();
    feature_json["id"] = feature->id;
    feature_json["expanded"] = feature->expanded;
    feature_json["root"] = feature->parent == nullptr;
    feature_json["type"] = to_string(feature->type);
    feature_json["name"] = feature->name;
    feature_json["state"] = to_string(feature->state);
    feature_json["suppressed"] = feature->suppressed;
    feature_json["dependencies"] = dependencies;

    if (const auto* point = std::get_if<model::PointObject>(&feature->object_data)) {
        feature_json["construction"] = point->construction;
        feature_json["pos"] = {point->x, point->y};
    } else if (const auto* line = std::get_if<model::LineObject>(&feature->object_data)) {
        feature_json["construction"] = line->construction;
        feature_json["point_a"] = line->point_a;
        feature_json["point_b"] = line->point_b;
    } else if (const auto* plane = std::get_if<model::PlaneObject>(&feature->object_data)) {
        feature_json["orginPoint"] = plane->origin_point;
        feature_json["vector"] = plane->vector_ref;
    }

    const auto payload_it = options.feature_payloads.find(feature->id);
    if (payload_it != options.feature_payloads.end()) {
        if (feature->type == model::FeatureType::SketchFeature) {
            if (payload_it->second.contains("id") && payload_it->second["id"].is_number_unsigned()) {
                feature_json["payload"] = ordered_json::object();
                feature_json["payload"]["id"] = payload_it->second["id"];
            }
            if (payload_it->second.contains("plane") && payload_it->second["plane"].is_number_unsigned()) {
                feature_json["plane"] = payload_it->second["plane"];
            }
        } else {
            feature_json["payload"] = payload_it->second;
        }

        if (feature->type == model::FeatureType::ExtrudeFeature) {
            if (payload_it->second.contains("sketch_id") && payload_it->second["sketch_id"].is_number_unsigned()) {
                feature_json["sketch_id"] = payload_it->second["sketch_id"];
            } else if (payload_it->second.contains("profile_id") && payload_it->second["profile_id"].is_number_unsigned()) {
                feature_json["sketch_id"] = payload_it->second["profile_id"];
            }
        }

        if (feature->type == model::FeatureType::FilletFeature &&
            payload_it->second.contains("feature_id") && payload_it->second["feature_id"].is_number_unsigned()) {
            feature_json["feature_id"] = payload_it->second["feature_id"];
        }
    } else if (feature->type == model::FeatureType::ExtrudeFeature && feature->parent != nullptr) {
        feature_json["sketch_id"] = feature->parent->id;
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

    if (options.extra_features.is_array()) {
        for (const auto& feature : options.extra_features) {
            if (feature.is_object()) {
                features.push_back(feature);
            }
        }
    }

    std::sort(features.begin(), features.end(), [](const ordered_json& lhs, const ordered_json& rhs) {
        const uint32_t lhs_id = lhs.contains("id") && lhs.at("id").is_number_unsigned() ? lhs.at("id").get<uint32_t>() : 0U;
        const uint32_t rhs_id = rhs.contains("id") && rhs.at("id").is_number_unsigned() ? rhs.at("id").get<uint32_t>() : 0U;
        return lhs_id < rhs_id;
    });

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
