#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "io/ModelDeserializer.hpp"
#include "io/ModelSerializer.hpp"
#include "model/FeatureTree.hpp"

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

    const nlohmann::json serialized = io::ModelSerializer::to_json(source, options);

    assert(serialized.contains("schema_version"));
    assert(serialized.contains("name"));
    assert(serialized.contains("units"));
    assert(serialized.contains("parameters"));
    assert(serialized.contains("features"));
    assert(serialized.contains("metadata"));
    assert(serialized.contains("session"));

    assert(serialized.at("parameters").at("depth") == "#height");
    assert(serialized.at("session").contains("camera"));

    const nlohmann::json& features = serialized.at("features");
    assert(features.is_array());
    assert(!features.empty());

    bool saw_expanded_flag = false;
    for (const auto& feature : features) {
        assert(feature.contains("id"));
        assert(feature.contains("type"));
        assert(feature.contains("name"));
        assert(feature.contains("dependencies"));
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
    test_missing_required_field();
    test_unknown_schema_version();
    test_circular_parameter_reference();
    test_unknown_feature_type();
    return 0;
}
