#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#endif

#include "app/Application.hpp"

#include <iostream>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <SDL3/SDL.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

#include "io/ModelDeserializer.hpp"
#include "io/ModelSerializer.hpp"

namespace {

app::ipc::MainOrchestrator g_orchestrator{};
constexpr float k_edge_pick_radius_px = 8.0f;
constexpr float k_edge_pick_world_scale = 35.0f;
constexpr float k_edge_pick_world_min = 0.02f;
constexpr float k_edge_pick_world_max = 8.0f;

std::optional<std::filesystem::path> first_existing_font_path(std::initializer_list<std::filesystem::path> candidates) {
    for (const auto& candidate : candidates) {
        std::error_code error_code;
        if (std::filesystem::exists(candidate, error_code)) {
            return candidate;
        }
    }
    return std::nullopt;
}

void configure_imgui_fonts(ImGuiIO& io) {
    io.Fonts->Clear();

    constexpr float k_body_size_px = 21.0f;
    ImFont* body_font = nullptr;

    const auto inter_font = first_existing_font_path({
        std::filesystem::path("assets/fonts/Inter-Regular.ttf"),
        std::filesystem::path("assets/env/Inter-Regular.ttf"),
        std::filesystem::path("C:/Windows/Fonts/Inter-Regular.ttf"),
        std::filesystem::path("C:/Windows/Fonts/inter.ttf"),
    });

    if (inter_font.has_value()) {
        body_font = io.Fonts->AddFontFromFileTTF(inter_font->string().c_str(), k_body_size_px);
    }

    if (body_font == nullptr) {
        const auto fallback_font = first_existing_font_path({
            std::filesystem::path("C:/Windows/Fonts/segoeui.ttf"),
            std::filesystem::path("C:/Windows/Fonts/arial.ttf"),
        });
        if (fallback_font.has_value()) {
            body_font = io.Fonts->AddFontFromFileTTF(fallback_font->string().c_str(), k_body_size_px);
        }
    }

    if (body_font == nullptr) {
        body_font = io.Fonts->AddFontDefault();
    }

    io.FontDefault = body_font;

    const auto icon_font = first_existing_font_path({
        std::filesystem::path("assets/fonts/segmdl2.ttf"),
        std::filesystem::path("assets/icons/segmdl2.ttf"),
        std::filesystem::path("C:/Windows/Fonts/segmdl2.ttf"),
    });

    if (icon_font.has_value()) {
        ImFontConfig icon_config{};
        icon_config.MergeMode = true;
        icon_config.PixelSnapH = true;
        icon_config.GlyphMinAdvanceX = 14.0f;
        static const ImWchar icon_ranges[] = {0xE700, 0xEAFF, 0};
        (void)io.Fonts->AddFontFromFileTTF(icon_font->string().c_str(), 15.0f, &icon_config, icon_ranges);
    }
}

std::filesystem::path project_root_settings_path() {
#if defined(VULCANCAD_PROJECT_ROOT)
    return std::filesystem::path(VULCANCAD_PROJECT_ROOT) / "settings.json";
#else
    return std::filesystem::path("settings.json");
#endif
}

uint32_t max_feature_id_in_tree(const model::FeatureTree& tree) {
    uint32_t max_id = 0U;
    if (const model::FeatureNode* root = tree.root()) {
        std::vector<const model::FeatureNode*> stack{root};
        while (!stack.empty()) {
            const model::FeatureNode* node = stack.back();
            stack.pop_back();
            max_id = std::max(max_id, node->id);
            for (const model::FeatureNode* child : node->children) {
                stack.push_back(child);
            }
        }
    }
    return max_id;
}

struct GeometryExportBundle {
    nlohmann::ordered_json extra_features = nlohmann::ordered_json::array();
    std::vector<uint32_t> line_feature_ids{};
    std::optional<uint32_t> plane_feature_id{};
};

std::vector<app::ipc::SketchSegment> build_sketch_segments_for_ipc(const sketch::SketchDocument& sketch_document) {
    constexpr float k_mm_to_world = 0.02f;

    const model::Plane& plane = sketch_document.plane();
    const glm::vec3 normal = glm::normalize(glm::length(plane.normal) < 1.0e-6f ? glm::vec3{0.0f, 0.0f, 1.0f} : plane.normal);
    const glm::vec3 helper = std::abs(normal.z) > 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{0.0f, 0.0f, 1.0f};
    const glm::vec3 u = glm::normalize(glm::cross(helper, normal));
    const glm::vec3 v = glm::normalize(glm::cross(normal, u));

    std::vector<app::ipc::SketchSegment> segments{};
    for (const sketch::SketchEntity& entity : sketch_document.entities()) {
        const auto* line = std::get_if<sketch::LineEntity>(&entity.data);
        if (line == nullptr) {
            continue;
        }

        const glm::vec3 a_world = plane.origin + u * (line->p1.x * k_mm_to_world) + v * (line->p1.y * k_mm_to_world);
        const glm::vec3 b_world = plane.origin + u * (line->p2.x * k_mm_to_world) + v * (line->p2.y * k_mm_to_world);

        segments.push_back(app::ipc::SketchSegment{
            static_cast<double>(a_world.x),
            static_cast<double>(a_world.y),
            static_cast<double>(a_world.z),
            static_cast<double>(b_world.x),
            static_cast<double>(b_world.y),
            static_cast<double>(b_world.z),
        });
    }

    return segments;
}

GeometryExportBundle build_geometry_export_bundle(const sketch::SketchDocument& sketch_document, uint32_t start_id) {
    GeometryExportBundle bundle{};
    uint32_t next_id = start_id;

    std::unordered_map<uint32_t, uint32_t> point_feature_ids{};
    std::optional<uint32_t> first_point_feature_id{};
    std::optional<uint32_t> second_point_feature_id{};

    for (const sketch::SketchEntity& entity : sketch_document.entities()) {
        const auto* point = std::get_if<sketch::PointEntity>(&entity.data);
        if (point == nullptr) {
            continue;
        }

        const uint32_t feature_id = next_id++;
        point_feature_ids[entity.id] = feature_id;
        if (!first_point_feature_id.has_value()) {
            first_point_feature_id = feature_id;
        } else if (!second_point_feature_id.has_value()) {
            second_point_feature_id = feature_id;
        }

        nlohmann::ordered_json feature = nlohmann::ordered_json::object();
        feature["id"] = feature_id;
        feature["expanded"] = true;
        feature["root"] = false;
        feature["type"] = "Point";
        feature["name"] = "Point." + std::to_string(feature_id);
        feature["state"] = "Valid";
        feature["suppressed"] = false;
        feature["dependencies"] = nlohmann::ordered_json::object();
        feature["construction"] = entity.construction;
        feature["pos"] = {point->pos.x, point->pos.y};
        bundle.extra_features.push_back(std::move(feature));
    }

    for (const sketch::SketchEntity& entity : sketch_document.entities()) {
        const auto* line = std::get_if<sketch::LineEntity>(&entity.data);
        if (line == nullptr) {
            continue;
        }

        const auto point_a_it = point_feature_ids.find(line->point_a);
        const auto point_b_it = point_feature_ids.find(line->point_b);
        if (point_a_it == point_feature_ids.end() || point_b_it == point_feature_ids.end()) {
            continue;
        }

        const uint32_t feature_id = next_id++;
        bundle.line_feature_ids.push_back(feature_id);

        nlohmann::ordered_json feature = nlohmann::ordered_json::object();
        feature["id"] = feature_id;
        feature["expanded"] = true;
        feature["root"] = false;
        feature["type"] = "Line";
        feature["name"] = "Line." + std::to_string(feature_id);
        feature["state"] = "Valid";
        feature["suppressed"] = false;
        feature["dependencies"] = nlohmann::ordered_json::object();
        feature["construction"] = entity.construction;
        feature["point_a"] = point_a_it->second;
        feature["point_b"] = point_b_it->second;
        bundle.extra_features.push_back(std::move(feature));
    }

    if (first_point_feature_id.has_value() && second_point_feature_id.has_value()) {
        const uint32_t feature_id = next_id++;
        bundle.plane_feature_id = feature_id;

        nlohmann::ordered_json plane_feature = nlohmann::ordered_json::object();
        plane_feature["id"] = feature_id;
        plane_feature["expanded"] = true;
        plane_feature["root"] = false;
        plane_feature["type"] = "Plane";
        plane_feature["name"] = "Plane." + std::to_string(feature_id);
        plane_feature["state"] = "Valid";
        plane_feature["suppressed"] = false;
        plane_feature["dependencies"] = nlohmann::ordered_json::object();
        plane_feature["orginPoint"] = *first_point_feature_id;
        plane_feature["vector"] = *second_point_feature_id;
        bundle.extra_features.push_back(std::move(plane_feature));
    }

    return bundle;
}

std::optional<std::string> open_file_picker(const char* title, const std::string& initial_dir) {
#if defined(_WIN32)
    OPENFILENAMEA dialog{};
    std::array<char, MAX_PATH> file_buffer{};
    const char filter[] = "JSON files (*.json)\0*.json\0All files (*.*)\0*.*\0\0";

    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = file_buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(file_buffer.size());
    dialog.lpstrInitialDir = initial_dir.empty() ? nullptr : initial_dir.c_str();
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if (GetOpenFileNameA(&dialog) == TRUE) {
        return std::string(file_buffer.data());
    }
#else
    (void)title;
    (void)initial_dir;
#endif
    return std::nullopt;
}

std::optional<std::string> save_file_picker(const char* title, const std::string& initial_dir) {
#if defined(_WIN32)
    OPENFILENAMEA dialog{};
    std::array<char, MAX_PATH> file_buffer{};
    const char filter[] = "JSON files (*.json)\0*.json\0All files (*.*)\0*.*\0\0";
    std::snprintf(file_buffer.data(), file_buffer.size(), "%s", "model.json");

    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = file_buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(file_buffer.size());
    dialog.lpstrInitialDir = initial_dir.empty() ? nullptr : initial_dir.c_str();
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    dialog.lpstrDefExt = "json";

    if (GetSaveFileNameA(&dialog) == TRUE) {
        return std::string(file_buffer.data());
    }
#else
    (void)title;
    (void)initial_dir;
#endif
    return std::nullopt;
}

void update_last_model_dir(std::string& current_dir, const std::string& file_path) {
    const std::filesystem::path path(file_path);
    std::filesystem::path directory = path;
    if (!path.has_extension() && !path.has_filename()) {
        directory = path;
    } else {
        directory = path.parent_path();
    }

    if (!directory.empty()) {
        current_dir = directory.string();
    }
}

}  // namespace

namespace app {

bool Application::init() {
    const std::filesystem::path settings_path = project_root_settings_path();
    if (!load_settings(settings_path.string().c_str())) {
        return false;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        last_error_ = "SDL_Init failed";
        return false;
    }
    sdl_initialized_ = true;

    if (!window_.create("VulcanCAD", settings_.width, settings_.height)) {
        last_error_ = std::string("Failed to create SDL3 window: ") + SDL_GetError();
        return false;
    }

    if (settings_.renderer == "vulkan") {
        if (!vulkan_context_.init(window_.native_handle(), settings_.vsync)) {
            last_error_ = "Failed to initialize Vulkan context";
            return false;
        }

        if (!render_frame_.init(
                vulkan_context_.device(),
                vulkan_context_.graphics_queue_family_index(),
                2)) {
            last_error_ = "Failed to initialize per-frame synchronization objects";
            return false;
        }
    }
#if defined(USE_DX12)
    else if (settings_.renderer == "dx12") {
        last_error_ = "DX12 backend is enabled, but not implemented in this bootstrap.";
        return false;
    }
#endif
    else {
        last_error_ = "Unsupported renderer configured in settings.json";
        return false;
    }

    if (!init_imgui()) {
        last_error_ = "Failed to initialize Dear ImGui";
        return false;
    }

    if (!g_orchestrator.init()) {
        last_error_ = "Failed to initialize runtime modules";
        return false;
    }

    if (!init_imgui_backend()) {
        last_error_ = "Failed to initialize Dear ImGui backend";
        return false;
    }

    feature_tree_panel_.set_feature_tree(&feature_tree_);
    feature_tree_panel_.set_sketch_document(&sketch_document_);
    if (!load_model_session("session/model.json")) {
        if (const model::FeatureNode* root = feature_tree_.root()) {
            model::FeatureTreeError tree_error = model::FeatureTreeError::Ok;
            const uint32_t sketch_id = feature_tree_.create_feature(model::FeatureType::SketchFeature, "Sketch.001", root->id, &tree_error);
            if (sketch_id != 0U && tree_error == model::FeatureTreeError::Ok) {
                (void)feature_tree_.create_feature(model::FeatureType::ExtrudeFeature, "Pad.001", root->id, nullptr);
                (void)feature_tree_.create_feature(model::FeatureType::FilletFeature, "Fillet.001", root->id, nullptr);
                (void)feature_tree_.create_feature(model::FeatureType::ShellFeature, "Shell.001", root->id, nullptr);
            }
        }
    }

    const std::filesystem::path initial_model_path = std::filesystem::path(settings_.last_model_dir) / "model.json";
    std::snprintf(save_model_path_buffer_.data(), save_model_path_buffer_.size(), "%s", initial_model_path.string().c_str());
    std::snprintf(open_model_path_buffer_.data(), open_model_path_buffer_.size(), "%s", initial_model_path.string().c_str());
    current_model_path_.clear();

    sketch_document_.solve();

    camera_.set_viewport_size(static_cast<float>(settings_.width), static_cast<float>(settings_.height));
    sync_camera_to_viewport();

    running_ = true;
    return true;
}

void Application::run() {
    while (running_) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            window_.process_event(event);
            const bool camera_mouse_event =
                event.type == SDL_EVENT_MOUSE_MOTION ||
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                event.type == SDL_EVENT_MOUSE_WHEEL;
            const bool camera_keyboard_event =
                event.type == SDL_EVENT_KEY_DOWN ||
                event.type == SDL_EVENT_KEY_UP;

            const auto is_point_over_viewport = [this](float x, float y) {
                const ImVec2 origin = viewport_panel_.content_origin();
                const ImVec2 size = viewport_panel_.content_size();
                if (size.x <= 1.0f || size.y <= 1.0f) {
                    return false;
                }

                return x >= origin.x && x <= origin.x + size.x && y >= origin.y && y <= origin.y + size.y;
            };

            if (camera_mouse_event) {
                float mouse_x = 0.0f;
                float mouse_y = 0.0f;
                if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    mouse_x = static_cast<float>(event.motion.x);
                    mouse_y = static_cast<float>(event.motion.y);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    mouse_x = static_cast<float>(event.button.x);
                    mouse_y = static_cast<float>(event.button.y);
                } else {
                    SDL_GetMouseState(&mouse_x, &mouse_y);
                }

                const bool over_viewport = is_point_over_viewport(mouse_x, mouse_y);
                const bool allow_camera_drag = !sketch_document_.is_active() && (over_viewport || camera_.is_interacting());
                const bool allow_camera_wheel = event.type == SDL_EVENT_MOUSE_WHEEL && over_viewport;

                if (allow_camera_drag || allow_camera_wheel) {
                    camera_.handle_event(event);
                }
            } else if (camera_keyboard_event && !sketch_document_.is_active()) {
                camera_.handle_event(event);
            }

            if (sketch_document_.is_active() && event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                const std::vector<app::ipc::SketchSegment> sketch_segments = build_sketch_segments_for_ipc(sketch_document_);
                sketch_document_.exit();
                app::ipc::GeometryRequest request{};
                request.command = app::ipc::GeometryCommand::RebuildFromSketch;
                request.sketch_segments = sketch_segments;
                (void)g_orchestrator.submit_once(request);
            }

            if (event.type == SDL_EVENT_QUIT) {
                running_ = false;
            }
        }

        if (window_.should_close()) {
            running_ = false;
        }

        if (window_.is_resized()) {
            const bool recreated = vulkan_context_.recreate_swapchain(
                window_.width(),
                window_.height(),
                settings_.vsync);
            camera_.set_viewport_size(static_cast<float>(window_.width()), static_cast<float>(window_.height()));
            sync_camera_to_viewport();
            window_.clear_resize_flag();
            if (!recreated) {
                running_ = false;
                continue;
            }
        }

        const app::ipc::GeometryResponse frame_response = g_orchestrator.submit_once(app::ipc::GeometryRequest{
            app::ipc::GeometryCommand::FrameUpdate,
            0U,
            false,
            0U,
        });
        if (!frame_response.success) {
            last_error_ = frame_response.message.empty() ? "Geometry module request failed" : frame_response.message;
        }

        if (!render_vulkan_frame()) {
            running_ = false;
        }
    }
}

void Application::shutdown() {
    persist_settings();
    g_orchestrator.shutdown();
    shutdown_imgui();

    render_frame_.shutdown();
    vulkan_context_.shutdown();

    window_.destroy();

    if (sdl_initialized_) {
        SDL_Quit();
        sdl_initialized_ = false;
    }

    running_ = false;
}

const std::string& Application::last_error() const {
    return last_error_;
}

bool Application::load_model_session(const char* path) {
    auto loaded = io::ModelDeserializer::from_file(path);
    if (!loaded.has_value()) {
        return false;
    }

    feature_tree_ = std::move(loaded.value());
    feature_tree_panel_.set_feature_tree(&feature_tree_);

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return true;
    }

    std::string data;
    file.seekg(0, std::ios::end);
    const std::streampos size = file.tellg();
    if (size <= 0) {
        return true;
    }

    data.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (!file.good() && !file.eof()) {
        return true;
    }

    const nlohmann::json root = nlohmann::json::parse(data, nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("session") || !root["session"].is_object()) {
        return true;
    }

    const nlohmann::json& session = root["session"];
    if (session.contains("camera") && session["camera"].is_object()) {
        camera_session_loaded_ = camera_.from_json(session["camera"]);
        if (camera_session_loaded_) {
            camera_.set_viewport_size(static_cast<float>(window_.width()), static_cast<float>(window_.height()));
            sync_camera_to_viewport();
        }
    }

    return true;
}

void Application::persist_model_session(const char* path) const {
    io::ModelSerializerOptions options{};
    options.pretty_print = true;
    options.model_name = "VulcanCAD Model";
    options.units = "mm";
    options.metadata = nlohmann::json::object();

    options.session = nlohmann::json::object();
    options.session["camera"] = camera_.to_json();

    (void)io::ModelSerializer::save_to_file(path, feature_tree_, options);
}

bool Application::load_feature_tree_session(const char* path) {
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

    return feature_tree_.restore_json_snapshot(data);
}

void Application::persist_feature_tree_session(const char* path) const {
    const std::filesystem::path session_path(path);
    std::error_code error_code;
    if (session_path.has_parent_path()) {
        std::filesystem::create_directories(session_path.parent_path(), error_code);
    }

    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return;
    }

    file << feature_tree_.to_json_snapshot();
}

bool Application::load_settings(const char* path) {
    std::filesystem::path settings_path(path);

    settings_path_ = settings_path.string();

    std::ifstream config_file(settings_path, std::ios::in | std::ios::binary);
    if (!config_file.is_open()) {
        last_error_ = "Could not open project-root settings.json";
        return false;
    }

    std::string data;
    config_file.seekg(0, std::ios::end);
    const std::streampos size = config_file.tellg();
    if (size <= 0) {
        last_error_ = "settings.json is empty";
        return false;
    }

    data.resize(static_cast<size_t>(size));
    config_file.seekg(0, std::ios::beg);
    config_file.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (!config_file.good() && !config_file.eof()) {
        last_error_ = "Failed while reading settings.json";
        return false;
    }

    const nlohmann::json root = nlohmann::json::parse(data, nullptr, false);
    if (root.is_discarded()) {
        last_error_ = "settings.json is not valid JSON";
        return false;
    }

    settings_.width = root.value("width", settings_.width);
    settings_.height = root.value("height", settings_.height);
    settings_.renderer = root.value("renderer", settings_.renderer);
    settings_.vsync = root.value("vsync", settings_.vsync);

    if (root.contains("window") && root["window"].is_object()) {
        const nlohmann::json& window = root["window"];
        settings_.width = window.value("width", settings_.width);
        settings_.height = window.value("height", settings_.height);
        settings_.vsync = window.value("vsync", settings_.vsync);
    }

    if (root.contains("io") && root["io"].is_object()) {
        const nlohmann::json& io_settings = root["io"];
        settings_.last_model_dir = io_settings.value("last_model_dir", settings_.last_model_dir);
    }

    return true;
}

void Application::persist_settings() const {
    if (settings_path_.empty()) {
        return;
    }

    nlohmann::json root = nlohmann::json::object();
    {
        std::ifstream input(settings_path_, std::ios::in | std::ios::binary);
        if (input.is_open()) {
            std::string data;
            input.seekg(0, std::ios::end);
            const std::streampos size = input.tellg();
            if (size > 0) {
                data.resize(static_cast<size_t>(size));
                input.seekg(0, std::ios::beg);
                input.read(data.data(), static_cast<std::streamsize>(data.size()));
                const nlohmann::json parsed = nlohmann::json::parse(data, nullptr, false);
                if (!parsed.is_discarded() && parsed.is_object()) {
                    root = parsed;
                }
            }
        }
    }

    root["width"] = settings_.width;
    root["height"] = settings_.height;
    root["renderer"] = settings_.renderer;
    root["vsync"] = settings_.vsync;
    root["io"]["last_model_dir"] = settings_.last_model_dir;

    const std::filesystem::path file_path(settings_path_);
    std::error_code error_code;
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path(), error_code);
    }

    std::ofstream output(settings_path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return;
    }

    output << root.dump(2);
}

bool Application::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "assets/env/imgui_layout.ini";
    configure_imgui_fonts(io);

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 7.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(10.0f, 9.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(9.0f, 7.0f);

    style.Colors[ImGuiCol_Text] = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.32f, 0.35f, 0.39f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.92f, 0.93f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.95f, 0.96f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.97f, 0.97f, 0.98f, 0.99f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.72f, 0.75f, 0.80f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.88f, 0.90f, 0.93f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.81f, 0.85f, 0.90f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.75f, 0.80f, 0.87f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.83f, 0.86f, 0.90f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.61f, 0.72f, 0.86f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.86f, 0.89f, 0.93f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.89f, 0.93f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.84f, 0.87f, 0.91f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.60f, 0.65f, 0.72f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.30f, 0.48f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.48f, 0.57f, 0.70f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.36f, 0.47f, 0.64f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.80f, 0.84f, 0.89f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.72f, 0.78f, 0.86f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.64f, 0.71f, 0.81f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.80f, 0.84f, 0.89f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.72f, 0.78f, 0.86f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.64f, 0.71f, 0.81f, 1.0f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.83f, 0.87f, 0.91f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.74f, 0.81f, 0.89f, 1.0f);
    style.Colors[ImGuiCol_TabSelected] = ImVec4(0.60f, 0.71f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_TabDimmed] = ImVec4(0.85f, 0.88f, 0.92f, 1.0f);
    style.Colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.71f, 0.78f, 0.87f, 1.0f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.90f, 0.92f, 0.95f, 1.0f);

    dock_layout_built_ = false;
    imgui_initialized_ = true;
    return true;
}

bool Application::init_imgui_backend() {
    if (!ImGui_ImplSDL3_InitForVulkan(window_.native_handle())) {
        return false;
    }

    if (imgui_descriptor_pool_.get() == VK_NULL_HANDLE) {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        };

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 2000;
        pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
        pool_info.pPoolSizes = pool_sizes;

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(vulkan_context_.device(), &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
            return false;
        }

        imgui_descriptor_pool_ = vk_wrap::descriptor_pool(vulkan_context_.device(), descriptor_pool);
    }

    imgui_color_attachment_formats_[0] = vulkan_context_.swapchain_format();
    imgui_pipeline_rendering_info_ = {};
    imgui_pipeline_rendering_info_.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    imgui_pipeline_rendering_info_.colorAttachmentCount = static_cast<uint32_t>(imgui_color_attachment_formats_.size());
    imgui_pipeline_rendering_info_.pColorAttachmentFormats = imgui_color_attachment_formats_.data();

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = vulkan_context_.instance();
    init_info.PhysicalDevice = vulkan_context_.physical_device();
    init_info.Device = vulkan_context_.device();
    init_info.QueueFamily = vulkan_context_.graphics_queue_family_index();
    init_info.Queue = vulkan_context_.graphics_queue();
    init_info.DescriptorPool = imgui_descriptor_pool_.get();
    init_info.DescriptorPoolSize = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = vulkan_context_.swapchain_image_count();
    init_info.UseDynamicRendering = true;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    init_info.MinAllocationSize = 1024 * 1024;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = imgui_pipeline_rendering_info_;
    init_info.PipelineInfoForViewports.PipelineRenderingCreateInfo = imgui_pipeline_rendering_info_;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        return false;
    }

    imgui_backend_initialized_ = true;
    return true;
}

void Application::shutdown_imgui() {
    if (!imgui_initialized_) {
        return;
    }

    if (imgui_backend_initialized_) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        imgui_backend_initialized_ = false;
    }

    ImGui::DestroyContext();
    imgui_initialized_ = false;
}

void Application::build_docked_layout() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(
        static_cast<float>(window_.width()),
        static_cast<float>(window_.height()));
    io.DeltaTime = 1.0f / 60.0f;

    ImGui::NewFrame();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr ImGuiWindowFlags host_flags = ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("MainDockHost", nullptr, host_flags);
    ImGui::PopStyleVar(2);

    const ImGuiID dockspace_id = ImGui::GetID("VulcanCADDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (!dock_layout_built_) {
        build_initial_dock_layout();
    }

    ImGui::End();

    draw_menu_bar();

    feature_tree_panel_.draw();
    viewport_panel_.set_sketch_plane(sketch_document_.plane());
    if (const sketch::GridFeature* grid = sketch_document_.active_grid_feature()) {
        viewport_panel_.set_grid_feature(*grid);
    } else {
        viewport_panel_.set_grid_feature(std::nullopt);
    }
    viewport_panel_.set_sketch_profiles(sketch_document_.extract_preview_profiles());

    viewport_panel_.draw();

#if defined(VULCANCAD_HAS_OCCT)
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && viewport_panel_.is_hovered()) {
        const std::optional<ui::ViewportPanel::Ray> ray = viewport_panel_.getClickRay(ImGui::GetMousePos());
        if (ray.has_value()) {
            app::ipc::GeometryRequest pick_request{};
            pick_request.command = app::ipc::GeometryCommand::PickSolid;
            pick_request.ray_origin_x = ray->origin.X();
            pick_request.ray_origin_y = ray->origin.Y();
            pick_request.ray_origin_z = ray->origin.Z();
            pick_request.ray_dir_x = ray->direction.X();
            pick_request.ray_dir_y = ray->direction.Y();
            pick_request.ray_dir_z = ray->direction.Z();
            // Convert small screen radius to world-space tolerance and scale it to CAD scene units.
            // Without scaling this value is often too small, making picks feel pixel-perfect.
            const float raw_tolerance_world = viewport_panel_.estimate_pick_tolerance_world(
                ImGui::GetMousePos(),
                k_edge_pick_radius_px);
            const float tuned_tolerance_world = std::clamp(
                raw_tolerance_world * k_edge_pick_world_scale,
                k_edge_pick_world_min,
                k_edge_pick_world_max);
            pick_request.edge_tolerance_mm = static_cast<double>(tuned_tolerance_world);

            const app::ipc::GeometryResponse pick_response = g_orchestrator.submit_once(pick_request);

            if (pick_response.success && pick_response.hit_solid_handle != geometry::k_invalid_solid_handle) {
                std::cout << "[APP] Picked solid handle=" << pick_response.hit_solid_handle
                          << ", feature_id=" << pick_response.hit_feature_id << std::endl;
                feature_tree_panel_.set_selected_feature(pick_response.hit_feature_id);
                viewport_panel_.set_selected_edges(pick_response.hit_edges);
                sketch_view_.set_external_snap_edges(pick_response.hit_edges);
            } else if (pick_response.success) {
                std::cout << "[APP] Pick success - no hit" << std::endl;
                feature_tree_panel_.set_selected_feature(0U);
                viewport_panel_.set_selected_edges({});
                sketch_view_.set_external_snap_edges({});
            } else {
                std::cout << "[APP] Pick failed - no hit" << std::endl;
                viewport_panel_.set_selected_edges({});
                sketch_view_.set_external_snap_edges({});
            }
        } else {
            std::cout << "[APP] Click detected but getClickRay returned nullopt" << std::endl;
        }
    }
#endif

    properties_panel_.draw();

    process_feature_tree_actions();

    sketch_view_.draw_overlay(
        sketch_document_,
        viewport_panel_.content_origin(),
        viewport_panel_.content_size(),
        vulkan_context_.view_projection_matrix());

    if (sketch_document_.is_active()) {
        sketch_document_.solve();
    }

    draw_status_bar();
    draw_worker_retry_popup();

    ImGui::Render();
}

void Application::process_feature_tree_actions() {
    const bool open_plane_properties = feature_tree_panel_.consume_open_plane_properties_request();
    if (open_plane_properties) {
        sketch_view_.request_open_plane_properties();
    }

    if (!open_plane_properties && feature_tree_panel_.consume_open_sketch_request()) {
        sketch_document_.enter();
        sketch_document_.solve();
    }

    const std::optional<ui::RebuildIntent> rebuild_intent = feature_tree_panel_.consume_rebuild_intent();
    if (!rebuild_intent.has_value()) {
        return;
    }

    model::RebuildRequest request{};
    request.full_rebuild = rebuild_intent->full_rebuild;
    request.start_feature_id = rebuild_intent->start_feature_id;
    (void)execute_feature_rebuild(request);
}

bool Application::execute_feature_rebuild(
    const model::RebuildRequest& request,
    const std::optional<app::ipc::GeometryRequest>& request_override,
    bool allow_retry_popup) {
    auto delegate = [&](const nlohmann::json& payload) -> model::RebuildDelegateResult {
        app::ipc::GeometryRequest typed_request{};
        typed_request.command = app::ipc::GeometryCommand::RebuildFeature;
        typed_request.feature_id = payload.value("feature_id", 0U);
        typed_request.full_rebuild = payload.value("full_rebuild", false);
        typed_request.start_feature_id = payload.value("start_feature_id", 0U);

        if (request_override.has_value()) {
            typed_request = *request_override;
        }

        const app::ipc::GeometryResponse response = g_orchestrator.submit_once(typed_request);
        if (response.success) {
            return {true, false, {}};
        }
        if (response.worker_crashed) {
            return {false, true, response.message.empty() ? "GeometryWorker crashed during rebuild" : response.message};
        }
        return {false, false, response.message.empty() ? "GeometryWorker failed rebuild request" : response.message};
    };

    const model::RebuildResult rebuild_result = feature_tree_.rebuild(
        request,
        delegate,
        [&]() {
            (void)g_orchestrator.submit_once(app::ipc::GeometryRequest{
                app::ipc::GeometryCommand::Tessellate,
                0U,
                false,
                0U,
            });
        },
        [&]() {
            (void)g_orchestrator.submit_once(app::ipc::GeometryRequest{
                app::ipc::GeometryCommand::UploadMesh,
                0U,
                false,
                0U,
            });
        },
        [&]() {
            (void)g_orchestrator.submit_once(app::ipc::GeometryRequest{
                app::ipc::GeometryCommand::Repaint,
                0U,
                false,
                0U,
            });
        });

    if (rebuild_result.success) {
        return true;
    }

    if (rebuild_result.worker_crashed) {
        if (!allow_retry_popup) {
            if (rebuild_result.failed_feature_id != 0U) {
                (void)feature_tree_.set_feature_state(rebuild_result.failed_feature_id, model::FeatureState::Error);
            }
            return false;
        }

        worker_retry_context_ = WorkerRetryContext{};
        worker_retry_context_->request = model::RebuildRequest{false, rebuild_result.failed_feature_id};
        worker_retry_context_->failed_feature_id = rebuild_result.failed_feature_id;
        worker_retry_context_->edited_request = app::ipc::GeometryRequest{
            app::ipc::GeometryCommand::RebuildFeature,
            rebuild_result.failed_feature_id,
            false,
            rebuild_result.failed_feature_id,
        };
        worker_retry_popup_opened_ = false;
        return false;
    }

    if (rebuild_result.failed_feature_id != 0U) {
        (void)feature_tree_.set_feature_state(rebuild_result.failed_feature_id, model::FeatureState::Error);
    }
    return false;
}

void Application::draw_worker_retry_popup() {
    if (!worker_retry_context_.has_value()) {
        return;
    }

    if (!worker_retry_popup_opened_) {
        ImGui::OpenPopup("Geometry Worker Crash");
        worker_retry_popup_opened_ = true;
    }

    ImGui::SetNextWindowSize(ImVec2(560.0f, 280.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Geometry Worker Crash", nullptr, ImGuiWindowFlags_NoResize)) {
        return;
    }

    ImGui::TextWrapped("GeometryWorker crashed while rebuilding feature %u. You can edit typed request fields and retry once.", worker_retry_context_->failed_feature_id);
    ImGui::Separator();

    int feature_id = static_cast<int>(worker_retry_context_->edited_request.feature_id);
    int start_feature_id = static_cast<int>(worker_retry_context_->edited_request.start_feature_id);
    bool full_rebuild = worker_retry_context_->edited_request.full_rebuild;

    ImGui::TextUnformatted("Command: RebuildFeature");
    ImGui::Checkbox("Full rebuild", &full_rebuild);
    ImGui::InputInt("Feature id", &feature_id);
    ImGui::InputInt("Start feature id", &start_feature_id);

    worker_retry_context_->edited_request.feature_id = feature_id <= 0 ? 0U : static_cast<uint32_t>(feature_id);
    worker_retry_context_->edited_request.start_feature_id = start_feature_id <= 0 ? 0U : static_cast<uint32_t>(start_feature_id);
    worker_retry_context_->edited_request.full_rebuild = full_rebuild;

    if (ImGui::Button("Retry Delegation")) {
        const model::RebuildRequest retry_request = worker_retry_context_->request;
        const app::ipc::GeometryRequest retry_worker_request = worker_retry_context_->edited_request;

        worker_retry_context_.reset();
        worker_retry_popup_opened_ = false;
        ImGui::CloseCurrentPopup();

        if (g_orchestrator.restart_worker()) {
            (void)execute_feature_rebuild(retry_request, retry_worker_request, false);
        } else {
            last_error_ = "Failed to restart GeometryWorker";
            if (retry_request.start_feature_id != 0U) {
                (void)feature_tree_.set_feature_state(retry_request.start_feature_id, model::FeatureState::Error);
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel And Mark Error")) {
        const uint32_t failed_feature_id = worker_retry_context_->failed_feature_id;
        worker_retry_context_.reset();
        worker_retry_popup_opened_ = false;
        ImGui::CloseCurrentPopup();
        if (failed_feature_id != 0U) {
            (void)feature_tree_.set_feature_state(failed_feature_id, model::FeatureState::Error);
        }
    }

    ImGui::EndPopup();
}

void Application::draw_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    const auto execute_model_save = [&](const std::string& save_path) -> bool {
        if (save_path.empty()) {
            last_error_ = "Model save path is empty";
            return false;
        }

        io::ModelSerializerOptions options{};
        options.pretty_print = save_model_pretty_print_;
        options.model_name = "VulcanCAD Model";
        options.units = "mm";
        options.metadata = nlohmann::json::object();
        options.metadata["source"] = "File->Save As";
        options.session = nlohmann::json::object();
        options.session["camera"] = camera_.to_json();
        const GeometryExportBundle geometry_bundle = build_geometry_export_bundle(sketch_document_, max_feature_id_in_tree(feature_tree_) + 1U);
        options.extra_features = geometry_bundle.extra_features;

        if (const model::FeatureNode* root = feature_tree_.root()) {
            std::vector<const model::FeatureNode*> stack{root};
            while (!stack.empty()) {
                const model::FeatureNode* node = stack.back();
                stack.pop_back();

                if (node->type == model::FeatureType::SketchFeature) {
                    if (!geometry_bundle.line_feature_ids.empty()) {
                        options.feature_payloads[node->id] = nlohmann::ordered_json::object();
                        options.feature_payloads[node->id]["id"] = geometry_bundle.line_feature_ids.front();
                        options.feature_payloads[node->id]["ids"] = nlohmann::ordered_json::array();
                        for (const uint32_t line_id : geometry_bundle.line_feature_ids) {
                            options.feature_payloads[node->id]["ids"].push_back(line_id);
                        }
                        if (geometry_bundle.plane_feature_id.has_value()) {
                            options.feature_payloads[node->id]["plane"] = *geometry_bundle.plane_feature_id;
                        }
                    }
                    break;
                }

                for (const model::FeatureNode* child : node->children) {
                    stack.push_back(child);
                }
            }
        }

        if (!io::ModelSerializer::save_to_file(save_path, feature_tree_, options)) {
            last_error_ = "Failed to save model JSON file";
            return false;
        }

        update_last_model_dir(settings_.last_model_dir, save_path);
        persist_settings();
        current_model_path_ = save_path;
        std::snprintf(save_model_path_buffer_.data(), save_model_path_buffer_.size(), "%s", save_path.c_str());
        std::snprintf(open_model_path_buffer_.data(), open_model_path_buffer_.size(), "%s", save_path.c_str());
        return true;
    };

    const auto execute_new_model = [&]() {
        feature_tree_ = model::FeatureTree{};
        feature_tree_panel_.set_feature_tree(&feature_tree_);

        sketch_document_ = sketch::SketchDocument(glm::vec3{0.0f, 0.0f, 0.0f});
        feature_tree_panel_.set_sketch_document(&sketch_document_);

        if (const model::FeatureNode* root = feature_tree_.root()) {
            model::FeatureTreeError tree_error = model::FeatureTreeError::Ok;
            const uint32_t sketch_id = feature_tree_.create_feature(model::FeatureType::SketchFeature, "Sketch.001", root->id, &tree_error);
            if (sketch_id != 0U && tree_error == model::FeatureTreeError::Ok) {
                (void)feature_tree_.create_feature(model::FeatureType::ExtrudeFeature, "Pad.001", root->id, nullptr);
                (void)feature_tree_.create_feature(model::FeatureType::FilletFeature, "Fillet.001", root->id, nullptr);
                (void)feature_tree_.create_feature(model::FeatureType::ShellFeature, "Shell.001", root->id, nullptr);
            }
        }

        current_model_path_.clear();
        const std::filesystem::path initial_model_path = std::filesystem::path(settings_.last_model_dir) / "model.json";
        std::snprintf(save_model_path_buffer_.data(), save_model_path_buffer_.size(), "%s", initial_model_path.string().c_str());
        std::snprintf(open_model_path_buffer_.data(), open_model_path_buffer_.size(), "%s", initial_model_path.string().c_str());
        last_error_.clear();
    };

    const auto invoke_save_as = [&]() {
#if defined(_WIN32)
        const std::optional<std::string> picked = save_file_picker("Save model JSON", settings_.last_model_dir);
        if (picked.has_value()) {
            (void)execute_model_save(*picked);
        }
#endif
#if !defined(_WIN32)
        {
            show_save_as_popup_ = true;
        }
#endif
    };

    const auto execute_model_open = [&](const std::string& open_path) {
        if (load_model_session(open_path.c_str())) {
            std::ifstream file(open_path, std::ios::in | std::ios::binary);
            if (file.is_open()) {
                std::string data;
                file.seekg(0, std::ios::end);
                const std::streampos size = file.tellg();
                if (size > 0) {
                    data.resize(static_cast<size_t>(size));
                    file.seekg(0, std::ios::beg);
                    file.read(data.data(), static_cast<std::streamsize>(data.size()));
                    const nlohmann::json root = nlohmann::json::parse(data, nullptr, false);
                    if (!root.is_discarded() && root.is_object() && root.contains("features") && root["features"].is_array()) {
                        std::unordered_map<uint32_t, const nlohmann::json*> feature_by_id{};
                        for (const auto& feature : root["features"]) {
                            if (!feature.is_object()) {
                                continue;
                            }
                            if (!feature.contains("id") || !feature["id"].is_number_unsigned()) {
                                continue;
                            }
                            feature_by_id[feature["id"].get<uint32_t>()] = &feature;
                        }

                        for (const auto& feature : root["features"]) {
                            if (!feature.is_object()) {
                                continue;
                            }

                            if (!feature.contains("type") || !feature["type"].is_string() || feature["type"].get<std::string>() != "SketchFeature") {
                                continue;
                            }

                            if (feature.contains("payload") && feature["payload"].is_object() &&
                                feature["payload"].contains("id") && feature["payload"]["id"].is_number_unsigned()) {
                                std::vector<uint32_t> referenced_ids{};
                                if (feature["payload"].contains("ids") && feature["payload"]["ids"].is_array()) {
                                    for (const auto& line_id : feature["payload"]["ids"]) {
                                        if (line_id.is_number_unsigned()) {
                                            referenced_ids.push_back(line_id.get<uint32_t>());
                                        }
                                    }
                                }
                                if (referenced_ids.empty()) {
                                    referenced_ids.push_back(feature["payload"]["id"].get<uint32_t>());
                                }

                                nlohmann::ordered_json entities = nlohmann::ordered_json::array();
                                std::unordered_set<uint32_t> emitted_points{};

                                for (const uint32_t ref_id : referenced_ids) {
                                    const auto line_it = feature_by_id.find(ref_id);
                                    if (line_it == feature_by_id.end() || line_it->second == nullptr) {
                                        continue;
                                    }

                                    const nlohmann::json& line_feature = *line_it->second;
                                    if (!line_feature.contains("point_a") || !line_feature["point_a"].is_number_unsigned() ||
                                        !line_feature.contains("point_b") || !line_feature["point_b"].is_number_unsigned()) {
                                        continue;
                                    }

                                    const uint32_t point_a = line_feature["point_a"].get<uint32_t>();
                                    const uint32_t point_b = line_feature["point_b"].get<uint32_t>();

                                    const auto emit_point = [&](uint32_t point_id) {
                                        if (emitted_points.contains(point_id)) {
                                            return;
                                        }

                                        const auto point_it = feature_by_id.find(point_id);
                                        if (point_it == feature_by_id.end() || point_it->second == nullptr) {
                                            return;
                                        }

                                        const nlohmann::json& point_feature = *point_it->second;
                                        if (!point_feature.contains("type") || !point_feature["type"].is_string() ||
                                            point_feature["type"].get<std::string>() != "Point") {
                                            return;
                                        }
                                        if (!point_feature.contains("pos") || !point_feature["pos"].is_array() || point_feature["pos"].size() != 2U ||
                                            !point_feature["pos"][0].is_number() || !point_feature["pos"][1].is_number()) {
                                            return;
                                        }

                                        nlohmann::ordered_json item = nlohmann::ordered_json::object();
                                        item["id"] = point_id;
                                        item["construction"] = point_feature.value("construction", false);
                                        item["type"] = "Point";
                                        item["pos"] = {point_feature["pos"][0].get<double>(), point_feature["pos"][1].get<double>()};
                                        entities.push_back(std::move(item));
                                        emitted_points.insert(point_id);
                                    };

                                    emit_point(point_a);
                                    emit_point(point_b);

                                    nlohmann::ordered_json line_item = nlohmann::ordered_json::object();
                                    line_item["id"] = ref_id;
                                    line_item["construction"] = line_feature.value("construction", false);
                                    line_item["type"] = "Line";
                                    line_item["point_a"] = point_a;
                                    line_item["point_b"] = point_b;
                                    entities.push_back(std::move(line_item));
                                }

                                if (!entities.empty()) {
                                    std::vector<nlohmann::ordered_json> sorted_entities{};
                                    sorted_entities.reserve(entities.size());
                                    for (auto& entity : entities) {
                                        sorted_entities.push_back(std::move(entity));
                                    }
                                    std::sort(sorted_entities.begin(), sorted_entities.end(), [](const auto& a, const auto& b) {
                                        return a["id"].template get<uint32_t>() < b["id"].template get<uint32_t>();
                                    });

                                    nlohmann::ordered_json sketch_payload = nlohmann::ordered_json::object();
                                    sketch_payload["entities"] = nlohmann::ordered_json::array();
                                    for (auto& entity : sorted_entities) {
                                        sketch_payload["entities"].push_back(std::move(entity));
                                    }

                                    std::string sketch_error{};
                                    if (!sketch_document_.apply_json_payload(sketch_payload, &sketch_error)) {
                                        last_error_ = sketch_error;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }

            update_last_model_dir(settings_.last_model_dir, open_path);
            persist_settings();
            current_model_path_ = open_path;
            std::snprintf(open_model_path_buffer_.data(), open_model_path_buffer_.size(), "%s", open_path.c_str());
            std::snprintf(save_model_path_buffer_.data(), save_model_path_buffer_.size(), "%s", open_path.c_str());
            return;
        }

        auto loaded = io::ModelDeserializer::from_file(open_path);
        if (loaded.has_value()) {
            feature_tree_ = std::move(loaded.value());
            feature_tree_panel_.set_feature_tree(&feature_tree_);
            update_last_model_dir(settings_.last_model_dir, open_path);
            persist_settings();
            current_model_path_ = open_path;
            std::snprintf(open_model_path_buffer_.data(), open_model_path_buffer_.size(), "%s", open_path.c_str());
            std::snprintf(save_model_path_buffer_.data(), save_model_path_buffer_.size(), "%s", open_path.c_str());
            return;
        }

        const io::ModelError& error = loaded.error();
        last_error_ = error.message;
    };

    const ImGuiIO& io = ImGui::GetIO();
    const bool allow_shortcuts = !io.WantTextInput && !ImGui::IsAnyItemActive();
    if (allow_shortcuts && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        execute_new_model();
    }
    if (allow_shortcuts && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
#if defined(_WIN32)
        const std::optional<std::string> picked = open_file_picker("Open model JSON", settings_.last_model_dir);
        if (picked.has_value()) {
            execute_model_open(*picked);
        }
#endif
#if !defined(_WIN32)
        {
            show_open_model_popup_ = true;
        }
#endif
    }
    if (allow_shortcuts && io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        invoke_save_as();
    } else if (allow_shortcuts && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        const bool has_existing_path = !current_model_path_.empty() && std::filesystem::exists(current_model_path_);
        if (has_existing_path) {
            (void)execute_model_save(current_model_path_);
        } else {
            invoke_save_as();
        }
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N")) {
            execute_new_model();
        }
        if (ImGui::MenuItem("Open Model...", "Ctrl+O")) {
#if defined(_WIN32)
            const std::optional<std::string> picked = open_file_picker("Open model JSON", settings_.last_model_dir);
            if (picked.has_value()) {
                execute_model_open(*picked);
            }
#endif
#if !defined(_WIN32)
            {
                show_open_model_popup_ = true;
            }
#endif
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            const bool has_existing_path = !current_model_path_.empty() && std::filesystem::exists(current_model_path_);
            if (has_existing_path) {
                (void)execute_model_save(current_model_path_);
            } else {
                invoke_save_as();
            }
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
            invoke_save_as();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            running_ = false;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, feature_tree_.can_undo())) {
            if (feature_tree_.undo()) {
                const model::FeatureNode* tree_root = feature_tree_.root();
                if (tree_root != nullptr) {
                    (void)execute_feature_rebuild(model::RebuildRequest{true, tree_root->id});
                }
            }
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, feature_tree_.can_redo())) {
            if (feature_tree_.redo()) {
                const model::FeatureNode* tree_root = feature_tree_.root();
                if (tree_root != nullptr) {
                    (void)execute_feature_rebuild(model::RebuildRequest{true, tree_root->id});
                }
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Insert")) {
        ImGui::MenuItem("Sketch");
        ImGui::MenuItem("Extrude");
        ImGui::MenuItem("Fillet");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        ImGui::MenuItem("Measure");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Shaded", nullptr, true);
        ImGui::MenuItem("Wireframe");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("AI")) {
        ImGui::MenuItem("Open AI Panel");
        ImGui::MenuItem("Submit Proposal");
        ImGui::EndMenu();
    }

    if (show_save_as_popup_) {
        ImGui::OpenPopup("Save Model As");
        show_save_as_popup_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(560.0f, 150.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Save Model As", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::InputText("Path", save_model_path_buffer_.data(), save_model_path_buffer_.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse##save_model")) {
            const std::optional<std::string> picked = save_file_picker("Save model JSON", settings_.last_model_dir);
            if (picked.has_value()) {
                std::snprintf(save_model_path_buffer_.data(), save_model_path_buffer_.size(), "%s", picked->c_str());
                update_last_model_dir(settings_.last_model_dir, *picked);
                persist_settings();
            }
        }
        ImGui::Checkbox("Pretty print JSON", &save_model_pretty_print_);
        if (ImGui::Button("Save")) {
            const std::string save_path = std::string(save_model_path_buffer_.data());
            if (execute_model_save(save_path)) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (show_open_model_popup_) {
        ImGui::OpenPopup("Open Model JSON");
        show_open_model_popup_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(560.0f, 170.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Open Model JSON", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::InputText("Path", open_model_path_buffer_.data(), open_model_path_buffer_.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse##open_model")) {
            const std::optional<std::string> picked = open_file_picker("Open model JSON", settings_.last_model_dir);
            if (picked.has_value()) {
                std::snprintf(open_model_path_buffer_.data(), open_model_path_buffer_.size(), "%s", picked->c_str());
                update_last_model_dir(settings_.last_model_dir, *picked);
                persist_settings();
            }
        }
        if (ImGui::Button("Load")) {
            const std::string open_path = std::string(open_model_path_buffer_.data());
            execute_model_open(open_path);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        ImGui::TextDisabled("Loads JSON and applies model to current working feature tree");
        ImGui::EndPopup();
    }

    ImGui::EndMainMenuBar();
}

void Application::draw_status_bar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float status_bar_height = 24.0f;

    ImGui::SetNextWindowPos(
        ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - status_bar_height));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, status_bar_height));
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr ImGuiWindowFlags status_flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::Begin("StatusBar", nullptr, status_flags);
    ImGui::PopStyleVar(3);

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Mouse: %.0f, %.0f", io.MousePos.x, io.MousePos.y);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    const size_t feature_count = feature_tree_.node_count() > 0U ? feature_tree_.node_count() - 1U : 0U;
    ImGui::Text("Features: %zu", feature_count);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("Camera: %s", camera_.state().projection_mode == scene::ProjectionMode::perspective ? "perspective" : "orthographic");

    if (sketch_document_.is_active()) {
        const int dof = sketch_document_.dof();
        ImVec4 dof_color = ImVec4(0.12f, 0.42f, 0.86f, 1.0f);
        if (dof == 0) {
            dof_color = ImVec4(0.12f, 0.62f, 0.22f, 1.0f);
        } else if (dof < 0) {
            dof_color = ImVec4(0.82f, 0.18f, 0.15f, 1.0f);
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::TextUnformatted("Sketch DOF:");
        ImGui::SameLine();
        ImGui::TextColored(dof_color, "%d", dof);
    }

    ImGui::End();
}

void Application::build_initial_dock_layout() {
    ImGui::DockBuilderRemoveNode(ImGui::GetID("VulcanCADDockspace"));
    const ImGuiID dockspace_id = ImGui::DockBuilderAddNode(
        ImGui::GetID("VulcanCADDockspace"),
        ImGuiDockNodeFlags_DockSpace);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    ImGuiID left_id = dockspace_id;
    ImGuiID center_id = dockspace_id;
    ImGuiID right_id = dockspace_id;

    const float viewport_width = viewport->Size.x > 1.0f ? viewport->Size.x : 1920.0f;
    const float left_ratio = 240.0f / viewport_width;
    const float right_ratio = 280.0f / viewport_width;

    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, left_ratio, &left_id, &center_id);
    ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, right_ratio, &right_id, &center_id);

    ImGui::DockBuilderDockWindow("FeatureTree", left_id);
    ImGui::DockBuilderDockWindow("Viewport", center_id);
    ImGui::DockBuilderDockWindow("Properties", right_id);

    ImGui::DockBuilderFinish(dockspace_id);
    dock_layout_built_ = true;
}

bool Application::render_vulkan_frame() {
    renderer::frame_resources& frame = render_frame_.current();

    uint32_t image_index = 0;
    if (!vulkan_context_.begin_frame(frame, &image_index)) {
        const bool recreated = vulkan_context_.recreate_swapchain(
            window_.width(),
            window_.height(),
            settings_.vsync);
        return recreated;
    }

    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplVulkan_NewFrame();

    build_docked_layout();

    sync_camera_to_viewport();

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.command_buffer);

    if (!vulkan_context_.end_frame(frame, image_index)) {
        return false;
    }

    render_frame_.advance();
    return true;
}

void Application::sync_camera_to_viewport() {
    const float width = static_cast<float>(std::max(window_.width(), 1U));
    const float height = static_cast<float>(std::max(window_.height(), 1U));
    const float aspect = width / height;

    const glm::mat4 view = camera_.view_matrix();
    const glm::mat4 projection = camera_.projection_matrix(aspect);
    const glm::mat4 view_projection = projection * view;

    vulkan_context_.set_camera_matrices(view, projection, view_projection);
    viewport_panel_.set_camera_matrices(view, projection, view_projection);
}

void Application::persist_camera_session() const {
    io::CameraSession::save("session/camera.json", camera_);
}

}  // namespace app
