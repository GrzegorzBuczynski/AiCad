#include "io/ModelDeserializer.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace io {

namespace {

constexpr const char* k_schema_version = "1.0";

struct ParsedFeature {
    uint32_t source_id = 0U;
    std::string name{};
    std::string type{};
    uint32_t parent_id = 0U;
    bool has_parent = false;
    bool root = false;
    bool suppressed = false;
    bool expanded = true;
    std::optional<uint32_t> chain_child_id{};
    nlohmann::json dependencies = nlohmann::json::array();
    model::FeatureObjectData object_data{};
};

model::FeatureType type_from_string(const std::string& value, bool* valid) {
    if (value == "PartContainer") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::PartContainer;
    }
    if (value == "SketchFeature") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::SketchFeature;
    }
    if (value == "Point") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::Point;
    }
    if (value == "Line") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::Line;
    }
    if (value == "Plane") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::Plane;
    }
    if (value == "ExtrudeFeature") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::ExtrudeFeature;
    }
    if (value == "RevolveFeature") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::RevolveFeature;
    }
    if (value == "FilletFeature") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::FilletFeature;
    }
    if (value == "ChamferFeature") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::ChamferFeature;
    }
    if (value == "ShellFeature") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::ShellFeature;
    }
    if (value == "HoleFeature") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::HoleFeature;
    }
    if (value == "MirrorFeature") {
        if (valid != nullptr) {
            *valid = true;
        }
        return model::FeatureType::MirrorFeature;
    }

    if (valid != nullptr) {
        *valid = false;
    }
    return model::FeatureType::SketchFeature;
}

bool is_schema_supported(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get<std::string>() == k_schema_version;
    }

    if (value.is_number_integer()) {
        return value.get<int>() == 1;
    }

    return false;
}

std::expected<nlohmann::json, ModelError> resolve_parameter_impl(
    const std::string& name,
    const nlohmann::json& input,
    std::unordered_map<std::string, nlohmann::json>& cache,
    std::unordered_set<std::string>& in_stack) {
    const auto cached = cache.find(name);
    if (cached != cache.end()) {
        return cached->second;
    }

    if (in_stack.contains(name)) {
        return std::unexpected(ModelError{
            ModelErrorCode::ParameterResolutionError,
            "Circular parameter reference detected: #" + name,
            {},
        });
    }

    if (!input.contains(name)) {
        return std::unexpected(ModelError{
            ModelErrorCode::ParameterResolutionError,
            "Missing parameter reference: #" + name,
            {},
        });
    }

    in_stack.insert(name);
    nlohmann::json resolved = input.at(name);

    if (resolved.is_string()) {
        const std::string text = resolved.get<std::string>();
        if (!text.empty() && text.front() == '#') {
            const std::string ref_name = text.substr(1);
            auto ref_resolved = resolve_parameter_impl(ref_name, input, cache, in_stack);
            if (!ref_resolved.has_value()) {
                return std::unexpected(ref_resolved.error());
            }
            resolved = ref_resolved.value();
        }
    }

    in_stack.erase(name);
    cache[name] = resolved;
    return resolved;
}

}  // namespace

std::expected<model::FeatureTree, ModelError> ModelDeserializer::from_string(const std::string& json_text) {
    const nlohmann::json root = nlohmann::json::parse(json_text, nullptr, false);
    if (root.is_discarded()) {
        return std::unexpected(ModelError{ModelErrorCode::ParseError, "Model JSON parse failed", {}});
    }

    if (!root.is_object()) {
        return std::unexpected(ModelError{ModelErrorCode::InvalidFormat, "Model root must be an object", {}});
    }

    if (!root.contains("schema_version") || !is_schema_supported(root.at("schema_version"))) {
        return std::unexpected(ModelError{ModelErrorCode::SchemaVersionError, "Unsupported schema_version", {}});
    }

    if (!root.contains("features") || !root.at("features").is_array()) {
        return std::unexpected(ModelError{ModelErrorCode::InvalidFormat, "Missing features[] array", {}});
    }

    if (root.contains("parameters")) {
        auto resolved_parameters = resolve_parameters(root.at("parameters"));
        if (!resolved_parameters.has_value()) {
            return std::unexpected(resolved_parameters.error());
        }
    }

    std::vector<ParsedFeature> parsed_features{};
    parsed_features.reserve(root.at("features").size());

    std::vector<FeatureIssue> issues{};
    for (const auto& item : root.at("features")) {
        if (!item.is_object()) {
            continue;
        }

        ParsedFeature parsed{};

        if (!item.contains("id") || !item.at("id").is_number_unsigned()) {
            issues.push_back({0U, "", "Feature missing unsigned id"});
            continue;
        }
        parsed.source_id = item.at("id").get<uint32_t>();

        if (!item.contains("name") || !item.at("name").is_string()) {
            issues.push_back({parsed.source_id, "", "Feature missing name"});
            continue;
        }
        parsed.name = item.at("name").get<std::string>();

        if (!item.contains("type") || !item.at("type").is_string()) {
            issues.push_back({parsed.source_id, parsed.name, "Feature missing type"});
            parsed.type = "SketchFeature";
        } else {
            parsed.type = item.at("type").get<std::string>();
        }

        if (item.contains("parent_id") && !item.at("parent_id").is_null()) {
            if (item.at("parent_id").is_number_unsigned()) {
                parsed.parent_id = item.at("parent_id").get<uint32_t>();
                parsed.has_parent = true;
            } else {
                issues.push_back({parsed.source_id, parsed.name, "Invalid parent_id"});
            }
        }

        if (item.contains("root") && item.at("root").is_boolean()) {
            parsed.root = item.at("root").get<bool>();
        }

        if (item.contains("dependencies")) {
            if (item.at("dependencies").is_array()) {
                parsed.dependencies = item.at("dependencies");
            } else if (item.at("dependencies").is_object() &&
                       item.at("dependencies").contains("id") &&
                       item.at("dependencies").at("id").is_number_unsigned()) {
                parsed.chain_child_id = item.at("dependencies").at("id").get<uint32_t>();
            }
        }

        if (parsed.type == "Point") {
            model::PointObject point{};
            if (item.contains("construction") && item.at("construction").is_boolean()) {
                point.construction = item.at("construction").get<bool>();
            }
            if (item.contains("pos") && item.at("pos").is_array() && item.at("pos").size() == 2U &&
                item.at("pos")[0].is_number() && item.at("pos")[1].is_number()) {
                point.x = item.at("pos")[0].get<double>();
                point.y = item.at("pos")[1].get<double>();
            }
            parsed.object_data = point;
        } else if (parsed.type == "Line") {
            model::LineObject line{};
            if (item.contains("construction") && item.at("construction").is_boolean()) {
                line.construction = item.at("construction").get<bool>();
            }
            if (item.contains("point_a") && item.at("point_a").is_number_unsigned()) {
                line.point_a = item.at("point_a").get<uint32_t>();
            }
            if (item.contains("point_b") && item.at("point_b").is_number_unsigned()) {
                line.point_b = item.at("point_b").get<uint32_t>();
            }
            parsed.object_data = line;
        } else if (parsed.type == "Plane") {
            model::PlaneObject plane{};
            if (item.contains("orginPoint") && item.at("orginPoint").is_number_unsigned()) {
                plane.origin_point = item.at("orginPoint").get<uint32_t>();
            }
            if (item.contains("vector") && item.at("vector").is_number_unsigned()) {
                plane.vector_ref = item.at("vector").get<uint32_t>();
            }
            parsed.object_data = plane;
        }

        if (item.contains("suppressed") && item.at("suppressed").is_boolean()) {
            parsed.suppressed = item.at("suppressed").get<bool>();
        }

        if (item.contains("expanded") && item.at("expanded").is_boolean()) {
            parsed.expanded = item.at("expanded").get<bool>();
        }

        parsed_features.push_back(std::move(parsed));
    }

    std::unordered_map<uint32_t, uint32_t> source_id_to_index{};
    source_id_to_index.reserve(parsed_features.size());
    for (uint32_t i = 0U; i < static_cast<uint32_t>(parsed_features.size()); ++i) {
        source_id_to_index[parsed_features[i].source_id] = i;
    }

    for (ParsedFeature& feature : parsed_features) {
        if (!feature.chain_child_id.has_value()) {
            continue;
        }

        const auto child_it = source_id_to_index.find(*feature.chain_child_id);
        if (child_it == source_id_to_index.end()) {
            issues.push_back({feature.source_id, feature.name, "dependencies.id references missing feature"});
            continue;
        }

        ParsedFeature& child = parsed_features[child_it->second];
        if (!child.has_parent) {
            child.parent_id = feature.source_id;
            child.has_parent = true;
        }
    }

    model::FeatureTree tree{};
    model::FeatureNode* model_root = tree.root();
    if (model_root == nullptr) {
        return std::unexpected(ModelError{ModelErrorCode::InvalidFormat, "Internal tree root not initialized", issues});
    }

    std::unordered_map<uint32_t, uint32_t> source_to_runtime{};
    source_to_runtime[model_root->id] = model_root->id;

    for (ParsedFeature& feature : parsed_features) {
        if (feature.type != "PartContainer") {
            continue;
        }
        if (!(feature.root || !feature.has_parent)) {
            continue;
        }

        model_root->name = feature.name;
        model_root->expanded = feature.expanded;
        model_root->suppressed = feature.suppressed;
        source_to_runtime[feature.source_id] = model_root->id;
        break;
    }

    std::unordered_set<uint32_t> pending{};
    for (size_t i = 0; i < parsed_features.size(); ++i) {
        pending.insert(static_cast<uint32_t>(i));
    }

    bool made_progress = true;
    while (!pending.empty() && made_progress) {
        made_progress = false;

        std::vector<uint32_t> to_remove{};
        for (const uint32_t idx : pending) {
            ParsedFeature& feature = parsed_features[idx];

            if (feature.type == "PartContainer" && source_to_runtime.contains(feature.source_id)) {
                to_remove.push_back(idx);
                made_progress = true;
                continue;
            }

            uint32_t runtime_parent_id = model_root->id;
            if (feature.has_parent) {
                const auto parent_it = source_to_runtime.find(feature.parent_id);
                if (parent_it == source_to_runtime.end()) {
                    continue;
                }
                runtime_parent_id = parent_it->second;
            }

            bool dependencies_ok = true;
            for (const auto& dep : feature.dependencies) {
                if (!dep.is_number_unsigned()) {
                    issues.push_back({feature.source_id, feature.name, "Invalid dependency id type"});
                    dependencies_ok = false;
                    continue;
                }

                const uint32_t dep_id = dep.get<uint32_t>();
                if (!source_to_runtime.contains(dep_id) && dep_id != feature.parent_id) {
                    dependencies_ok = false;
                }
            }

            if (!dependencies_ok) {
                continue;
            }

            bool valid_type = false;
            const model::FeatureType type = type_from_string(feature.type, &valid_type);

            model::FeatureTreeError create_error = model::FeatureTreeError::Ok;
            uint32_t created_id = tree.create_feature(type, feature.name, runtime_parent_id, &create_error);
            if (created_id == 0U || create_error != model::FeatureTreeError::Ok) {
                issues.push_back({feature.source_id, feature.name, "Could not create feature in tree"});
                to_remove.push_back(idx);
                made_progress = true;
                continue;
            }

            source_to_runtime[feature.source_id] = created_id;

            model::FeatureNode* created_feature = tree.find_feature(created_id);
            if (created_feature != nullptr) {
                created_feature->expanded = feature.expanded;
                created_feature->object_data = feature.object_data;
            }

            if (feature.suppressed) {
                (void)tree.set_suppressed(created_id, true);
            }

            if (!valid_type) {
                issues.push_back({feature.source_id, feature.name, "Unknown feature type: " + feature.type});
                (void)tree.set_feature_state(created_id, model::FeatureState::Error);
            }

            to_remove.push_back(idx);
            made_progress = true;
        }

        for (const uint32_t idx : to_remove) {
            pending.erase(idx);
        }
    }

    if (!pending.empty()) {
        for (const uint32_t idx : pending) {
            const ParsedFeature& feature = parsed_features[idx];
            issues.push_back({feature.source_id, feature.name, "Broken feature dependency graph"});
        }

        return std::unexpected(ModelError{
            ModelErrorCode::DependencyGraphError,
            "Broken feature dependency graph",
            issues,
        });
    }

    return tree;
}

std::expected<model::FeatureTree, ModelError> ModelDeserializer::from_file(const std::string& path) {
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        return std::unexpected(ModelError{ModelErrorCode::IoError, "Could not open model file", {}});
    }

    std::string data;
    input.seekg(0, std::ios::end);
    const std::streampos size = input.tellg();
    if (size <= 0) {
        return std::unexpected(ModelError{ModelErrorCode::IoError, "Model file is empty", {}});
    }

    data.resize(static_cast<size_t>(size));
    input.seekg(0, std::ios::beg);
    input.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (!input.good() && !input.eof()) {
        return std::unexpected(ModelError{ModelErrorCode::IoError, "Could not read model file", {}});
    }

    return from_string(data);
}

std::expected<nlohmann::json, ModelError> ModelDeserializer::resolve_parameters(const nlohmann::json& parameters) {
    if (!parameters.is_object()) {
        return std::unexpected(ModelError{ModelErrorCode::InvalidFormat, "parameters must be a JSON object", {}});
    }

    nlohmann::json resolved = nlohmann::json::object();
    std::unordered_map<std::string, nlohmann::json> cache{};
    std::unordered_set<std::string> in_stack{};

    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        const std::string key = it.key();
        auto value = resolve_parameter_impl(key, parameters, cache, in_stack);
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        resolved[key] = value.value();
    }

    return resolved;
}

}  // namespace io
