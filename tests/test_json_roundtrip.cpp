#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "io/ModelDeserializer.hpp"
#include "io/ModelSerializer.hpp"
#include "model/FeatureTree.hpp"
#include "sketch/SketchDocument.hpp"

namespace {

model::FeatureTree make_demo_tree() {
    model::FeatureTree tree{};
    model::FeatureNode* root = tree.root();
    assert(root != nullptr);

    model::FeatureTreeError error = model::FeatureTreeError::Ok;
    const uint32_t sketch_id = tree.create_feature(model::FeatureType::SketchFeature, "Sketch.001", root->id, &error);
    assert(sketch_id != 0U && error == model::FeatureTreeError::Ok);

    const uint32_t extrude_id = tree.create_feature(model::FeatureType::ExtrudeFeature, "Pad.001", root->id, &error);
    assert(extrude_id != 0U && error == model::FeatureTreeError::Ok);

    model::FeatureNode* sketch = tree.find_feature(sketch_id);
    model::FeatureNode* extrude = tree.find_feature(extrude_id);
    assert(sketch != nullptr);
    assert(extrude != nullptr);

    root->expanded = false;
    sketch->expanded = false;
    extrude->expanded = true;

    return tree;
}

const model::FeatureNode* find_feature_by_name(const model::FeatureNode* start, const std::string& name) {
    if (start == nullptr) {
        return nullptr;
    }

    if (start->name == name) {
        return start;
    }

    for (const model::FeatureNode* child : start->children) {
        const model::FeatureNode* found = find_feature_by_name(child, name);
        if (found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

void test_serializer_emits_required_fields() {
    model::FeatureTree source = make_demo_tree();

    io::ModelSerializerOptions options{};
    options.pretty_print = true;
    options.parameters = {
        {"height", 15.0},
        {"depth", "#height"},
    };
    options.metadata = {
        {"author", "test"},
    };
    options.session = {
        {"camera", {
            {"projection_mode", "perspective"},
            {"fov", 60.0},
        }},
    };

    nlohmann::ordered_json point_feature = nlohmann::ordered_json::object();
    point_feature["id"] = 3U;
    point_feature["expanded"] = true;
    point_feature["root"] = false;
    point_feature["type"] = "Point";
    point_feature["name"] = "Point.001";
    point_feature["state"] = "Valid";
    point_feature["suppressed"] = false;
    point_feature["dependencies"] = nlohmann::ordered_json::object();
    point_feature["construction"] = false;
    point_feature["pos"] = {-57.0, -79.0};

    nlohmann::ordered_json line_feature = nlohmann::ordered_json::object();
    line_feature["id"] = 4U;
    line_feature["expanded"] = true;
    line_feature["root"] = false;
    line_feature["type"] = "Line";
    line_feature["name"] = "Line.001";
    line_feature["state"] = "Valid";
    line_feature["suppressed"] = false;
    line_feature["dependencies"] = nlohmann::ordered_json::object();
    line_feature["construction"] = false;
    line_feature["point_a"] = 3U;
    line_feature["point_b"] = 5U;

    nlohmann::ordered_json plane_feature = nlohmann::ordered_json::object();
    plane_feature["id"] = 6U;
    plane_feature["expanded"] = true;
    plane_feature["root"] = false;
    plane_feature["type"] = "Plane";
    plane_feature["name"] = "Plane.001";
    plane_feature["state"] = "Valid";
    plane_feature["suppressed"] = false;
    plane_feature["dependencies"] = nlohmann::ordered_json::object();
    plane_feature["orginPoint"] = 3U;
    plane_feature["vector"] = 5U;

    options.extra_features = nlohmann::ordered_json::array({point_feature, line_feature, plane_feature});
    options.feature_payloads[2] = {{"id", 4U}, {"plane", 6U}};

    const std::string serialized_text = io::ModelSerializer::to_string(source, options);
    const nlohmann::ordered_json serialized = nlohmann::ordered_json::parse(serialized_text);

    assert(serialized.contains("schema_version"));
    assert(serialized.contains("name"));
    assert(serialized.contains("units"));
    assert(serialized.contains("parameters"));
    assert(serialized.contains("features"));
    assert(serialized.contains("metadata"));
    assert(serialized.contains("session"));

    assert(serialized.at("parameters").at("depth") == "#height");
    assert(serialized.at("session").contains("camera"));

    const nlohmann::ordered_json& features = serialized.at("features");
    assert(features.is_array());
    assert(!features.empty());

    const nlohmann::ordered_json& first_feature = features.at(0);
    std::vector<std::string> first_feature_keys{};
    for (auto it = first_feature.begin(); it != first_feature.end(); ++it) {
        first_feature_keys.push_back(it.key());
    }
    assert(first_feature_keys.size() >= 8U);
    assert(first_feature_keys[0] == "id");
    assert(first_feature_keys[1] == "expanded");
    assert(first_feature_keys[2] == "root");
    assert(first_feature_keys[3] == "type");

    bool saw_expanded_flag = false;
    for (const auto& feature : features) {
        assert(feature.contains("id"));
        assert(feature.contains("type"));
        assert(feature.contains("name"));
        assert(feature.contains("dependencies"));
        assert(feature.at("dependencies").is_object());
        assert(feature.contains("root"));
        assert(feature.contains("expanded"));
        if (feature.contains("expanded") && feature.at("expanded").is_boolean()) {
            saw_expanded_flag = true;
        }
    }
    assert(saw_expanded_flag);
}

void test_roundtrip_preserves_feature_truth_fields() {
    model::FeatureTree source = make_demo_tree();

    io::ModelSerializerOptions options{};
    options.pretty_print = true;
    options.parameters = {
        {"height", 15.0},
        {"depth", "#height"},
    };

    const std::string serialized = io::ModelSerializer::to_string(source, options);
    auto restored = io::ModelDeserializer::from_string(serialized);
    assert(restored.has_value());

    const model::FeatureTree& restored_tree = restored.value();
    const model::FeatureNode* restored_root = restored_tree.root();
    assert(restored_root != nullptr);

    const model::FeatureNode* restored_sketch = find_feature_by_name(restored_root, "Sketch.001");
    const model::FeatureNode* restored_extrude = find_feature_by_name(restored_root, "Pad.001");
    assert(restored_sketch != nullptr);
    assert(restored_extrude != nullptr);

    assert(restored_root->expanded == false);
    assert(restored_sketch->expanded == false);
    assert(restored_extrude->expanded == true);

    assert(restored_sketch->parent == restored_root);
    assert(restored_extrude->parent == restored_root);
}

void test_roundtrip_preserves_topology_count() {
    model::FeatureTree source = make_demo_tree();
    const size_t source_count = source.node_count();

    io::ModelSerializerOptions options{};
    const std::string serialized = io::ModelSerializer::to_string(source, options);
    auto restored = io::ModelDeserializer::from_string(serialized);
    assert(restored.has_value());

    assert(restored.value().node_count() == source_count);
}

void test_serializer_sketch_payload_contains_lines() {
    model::FeatureTree source = make_demo_tree();

    sketch::SketchDocument sketch({0.0f, 0.0f, 0.0f});
    const sketch::entity_id line_id = sketch.add_line({-57.0f, -79.0f}, {37.0f, 82.0f});
    const nlohmann::ordered_json sketch_payload = sketch.to_json_payload();

    const nlohmann::ordered_json& entities = sketch_payload.at("entities");
    assert(entities.is_array());
    assert(entities.size() == 3U);

    const nlohmann::ordered_json& point_entity = entities.at(0);
    const nlohmann::ordered_json& line_entity = entities.at(2);

    std::vector<std::string> point_keys{};
    for (auto it = point_entity.begin(); it != point_entity.end(); ++it) {
        point_keys.push_back(it.key());
    }
    assert(point_keys.size() >= 4U);
    assert(point_keys[0] == "id");
    assert(point_keys[1] == "construction");
    assert(point_keys[2] == "type");

    std::vector<std::string> line_keys{};
    for (auto it = line_entity.begin(); it != line_entity.end(); ++it) {
        line_keys.push_back(it.key());
    }
    assert(line_keys.size() >= 5U);
    assert(line_keys[0] == "id");
    assert(line_keys[1] == "construction");
    assert(line_keys[2] == "type");
    assert(line_entity.at("point_a").is_number_unsigned());
    assert(line_entity.at("point_b").is_number_unsigned());

    sketch::SketchDocument restored({0.0f, 0.0f, 0.0f});
    std::string apply_error{};
    assert(restored.apply_json_payload(sketch_payload, &apply_error));
    const auto restored_line_points = restored.line_points(line_id);
    assert(restored_line_points.has_value());
    assert(restored_line_points->first == 1U);
    assert(restored_line_points->second == 2U);

    io::ModelSerializerOptions options{};
    options.pretty_print = true;
    options.feature_payloads[2] = {{"id", line_id}, {"plane", line_id + 3U}};
    options.extra_features = nlohmann::ordered_json::array();

    for (const auto& entity : sketch_payload.at("entities")) {
        if (!entity.is_object() || !entity.contains("type") || !entity["type"].is_string()) {
            continue;
        }

        if (entity["type"].get<std::string>() == "Point") {
            nlohmann::ordered_json point_feature = nlohmann::ordered_json::object();
            point_feature["id"] = entity["id"];
            point_feature["expanded"] = true;
            point_feature["root"] = false;
            point_feature["type"] = "Point";
            point_feature["name"] = "Point." + std::to_string(entity["id"].get<uint32_t>());
            point_feature["state"] = "Valid";
            point_feature["suppressed"] = false;
            point_feature["dependencies"] = nlohmann::ordered_json::object();
            point_feature["construction"] = entity.value("construction", false);
            point_feature["pos"] = entity.at("pos");
            options.extra_features.push_back(std::move(point_feature));
        } else if (entity["type"].get<std::string>() == "Line") {
            nlohmann::ordered_json line_feature = nlohmann::ordered_json::object();
            line_feature["id"] = entity["id"];
            line_feature["expanded"] = true;
            line_feature["root"] = false;
            line_feature["type"] = "Line";
            line_feature["name"] = "Line." + std::to_string(entity["id"].get<uint32_t>());
            line_feature["state"] = "Valid";
            line_feature["suppressed"] = false;
            line_feature["dependencies"] = nlohmann::ordered_json::object();
            line_feature["construction"] = entity.value("construction", false);
            line_feature["point_a"] = entity.at("point_a");
            line_feature["point_b"] = entity.at("point_b");
            options.extra_features.push_back(std::move(line_feature));
        }
    }

    nlohmann::ordered_json plane_feature = nlohmann::ordered_json::object();
    plane_feature["id"] = line_id + 3U;
    plane_feature["expanded"] = true;
    plane_feature["root"] = false;
    plane_feature["type"] = "Plane";
    plane_feature["name"] = "Plane.001";
    plane_feature["state"] = "Valid";
    plane_feature["suppressed"] = false;
    plane_feature["dependencies"] = nlohmann::ordered_json::object();
    plane_feature["orginPoint"] = 1U;
    plane_feature["vector"] = 2U;
    options.extra_features.push_back(std::move(plane_feature));

    const nlohmann::ordered_json serialized = io::ModelSerializer::to_json(source, options);
    assert(serialized.contains("features"));
    assert(serialized["features"].is_array());

    bool found_sketch_payload = false;
    for (const auto& feature : serialized["features"]) {
        if (!feature.contains("type") || !feature["type"].is_string()) {
            continue;
        }

        if (feature["type"].get<std::string>() != "SketchFeature") {
            continue;
        }

        assert(feature.contains("payload"));
        assert(feature["payload"].is_object());
        assert(feature["payload"].contains("id"));
        assert(feature["payload"].at("id").is_number_unsigned());
        assert(feature["payload"].at("id") == 3U);
        assert(feature.contains("plane"));
        assert(feature["plane"].is_number_unsigned());
        assert(feature["plane"] == 6U);
        assert(!feature["payload"].contains("entities"));

        found_sketch_payload = true;
        break;
    }

    assert(found_sketch_payload);

    bool saw_point = false;
    bool saw_line = false;
    bool saw_plane = false;
    for (const auto& feature : serialized["features"]) {
        if (!feature.contains("type") || !feature["type"].is_string()) {
            continue;
        }

        if (feature["type"].get<std::string>() == "Point") {
            saw_point = true;
            assert(feature.contains("pos"));
            assert(feature.at("pos").is_array());
        }
        if (feature["type"].get<std::string>() == "Line") {
            saw_line = true;
            assert(feature.contains("point_a"));
            assert(feature.contains("point_b"));
        }
        if (feature["type"].get<std::string>() == "Plane") {
            saw_plane = true;
            assert(feature.contains("orginPoint"));
            assert(feature.contains("vector"));
        }
    }

    assert(saw_point);
    assert(saw_line);
    assert(saw_plane);
}

void test_missing_required_field() {
    const nlohmann::json invalid = {
        {"schema_version", "1.0"},
        {"parameters", nlohmann::json::object()},
        {"metadata", nlohmann::json::object()},
    };

    auto restored = io::ModelDeserializer::from_string(invalid.dump());
    assert(!restored.has_value());
    assert(restored.error().code == io::ModelErrorCode::InvalidFormat);
}

void test_unknown_schema_version() {
    const nlohmann::json invalid = {
        {"schema_version", "999.0"},
        {"name", "BadSchema"},
        {"units", "mm"},
        {"parameters", nlohmann::json::object()},
        {"features", nlohmann::json::array()},
        {"metadata", nlohmann::json::object()},
    };

    auto restored = io::ModelDeserializer::from_string(invalid.dump());
    assert(!restored.has_value());
    assert(restored.error().code == io::ModelErrorCode::SchemaVersionError);
}

void test_circular_parameter_reference() {
    const nlohmann::json parameters = {
        {"a", "#b"},
        {"b", "#a"},
    };

    auto resolved = io::ModelDeserializer::resolve_parameters(parameters);
    assert(!resolved.has_value());
    assert(resolved.error().code == io::ModelErrorCode::ParameterResolutionError);
}

void test_unknown_feature_type() {
    const nlohmann::json model = {
        {"schema_version", "1.0"},
        {"name", "UnknownType"},
        {"units", "mm"},
        {"parameters", nlohmann::json::object()},
        {"metadata", nlohmann::json::object()},
        {"features", nlohmann::json::array({
            {
                {"id", 1},
                {"type", "PartContainer"},
                {"name", "Part.001"},
                {"parent_id", nullptr},
                {"dependencies", nlohmann::json::array()},
            },
            {
                {"id", 2},
                {"type", "UnknownFancyFeature"},
                {"name", "Broken.001"},
                {"parent_id", 1},
                {"dependencies", nlohmann::json::array({1})},
            },
        })},
    };

    auto restored = io::ModelDeserializer::from_string(model.dump());
    assert(restored.has_value());

    model::FeatureTree tree = std::move(restored.value());
    const model::FeatureNode* root = tree.root();
    assert(root != nullptr);
    assert(!root->children.empty());
    assert(root->children.front()->state == model::FeatureState::Error);
}

}  // namespace

int main() {
    test_serializer_emits_required_fields();
    test_roundtrip_preserves_feature_truth_fields();
    test_roundtrip_preserves_topology_count();
    test_serializer_sketch_payload_contains_lines();
    test_missing_required_field();
    test_unknown_schema_version();
    test_circular_parameter_reference();
    test_unknown_feature_type();
    return 0;
}
