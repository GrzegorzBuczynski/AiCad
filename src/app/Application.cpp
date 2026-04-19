#include "app/Application.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

#include <SDL3/SDL.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

namespace {

enum class worker_result : uint8_t {
    ok,
    crashed,
    failed
};

class GeometryWorker {
public:
    bool start() {
        running_ = true;
        return true;
    }

    void stop() {
        running_ = false;
    }

    worker_result execute(std::string_view payload) {
        if (!running_) {
            return worker_result::crashed;
        }

        if (payload.empty()) {
            return worker_result::failed;
        }

        return worker_result::ok;
    }

private:
    bool running_ = false;
};

class MainOrchestrator {
public:
    bool init() {
        return worker_.start();
    }

    void shutdown() {
        worker_.stop();
    }

    worker_result submit_geometry_request_once(const std::string& payload) {
        return worker_.execute(payload);
    }

    bool restart_worker() {
        worker_.stop();
        return worker_.start();
    }

private:
    GeometryWorker worker_{};
};

MainOrchestrator g_orchestrator{};

}  // namespace

namespace app {

bool Application::init() {
    if (!load_settings("settings.json")) {
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
    if (const model::FeatureNode* root = feature_tree_.root()) {
        model::FeatureTreeError tree_error = model::FeatureTreeError::Ok;
        const uint32_t sketch_id = feature_tree_.create_feature(model::FeatureType::SketchFeature, "Sketch.001", root->id, &tree_error);
        if (sketch_id != 0U && tree_error == model::FeatureTreeError::Ok) {
            (void)feature_tree_.create_feature(model::FeatureType::ExtrudeFeature, "Pad.001", root->id, nullptr);
            (void)feature_tree_.create_feature(model::FeatureType::FilletFeature, "Fillet.001", root->id, nullptr);
            (void)feature_tree_.create_feature(model::FeatureType::ShellFeature, "Shell.001", root->id, nullptr);
        }
    }

    if (io::CameraSession::load("session/camera.json", camera_)) {
        camera_session_loaded_ = true;
    }

    sketch_document_.add_line({-30.0f, -20.0f}, {30.0f, -20.0f});
    sketch_document_.add_line({30.0f, -20.0f}, {30.0f, 20.0f});
    sketch_document_.add_line({30.0f, 20.0f}, {-30.0f, 20.0f});
    sketch_document_.add_line({-30.0f, 20.0f}, {-30.0f, -20.0f});
    sketch_document_.add_constraint(sketch::HorizontalConstraint{1U});
    sketch_document_.add_constraint(sketch::HorizontalConstraint{3U});
    sketch_document_.add_constraint(sketch::VerticalConstraint{2U});
    sketch_document_.add_constraint(sketch::VerticalConstraint{4U});
    sketch_document_.add_constraint(sketch::DistanceDim{1U, 0U, 1U, 1U, 60.0f});
    sketch_document_.add_constraint(sketch::DistanceDim{1U, 1U, 2U, 1U, 40.0f});
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
                sketch_document_.exit();
                (void)g_orchestrator.submit_geometry_request_once("rebuild_dependent_features_from_sketch");
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

        if (g_orchestrator.submit_geometry_request_once("frame_update_payload") != worker_result::ok) {
            last_error_ = "Geometry module request failed after restart attempt";
        }

        if (!render_vulkan_frame()) {
            running_ = false;
        }
    }
}

void Application::shutdown() {
    persist_camera_session();
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

bool Application::load_settings(const char* path) {
    std::filesystem::path settings_path(path);
    if (!std::filesystem::exists(settings_path)) {
        const std::filesystem::path parent_settings = std::filesystem::path("..") / path;
        const std::filesystem::path grandparent_settings = std::filesystem::path("..") / ".." / path;

        if (std::filesystem::exists(parent_settings)) {
            settings_path = parent_settings;
        } else if (std::filesystem::exists(grandparent_settings)) {
            settings_path = grandparent_settings;
        }
    }

    std::ifstream config_file(settings_path, std::ios::in | std::ios::binary);
    if (!config_file.is_open()) {
        last_error_ = "Could not open settings.json in current, parent, or grandparent directory";
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

    return true;
}

bool Application::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "assets/env/imgui_layout.ini";

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.95f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.98f, 0.98f, 0.99f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.91f, 0.93f, 0.96f, 1.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.97f, 0.98f, 1.00f, 0.98f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.84f, 0.88f, 0.94f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.77f, 0.84f, 0.93f, 1.0f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.93f, 0.95f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.80f, 0.86f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.82f, 0.96f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.56f, 0.74f, 0.94f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.92f, 0.95f, 0.99f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.84f, 0.90f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.77f, 0.86f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.86f, 0.90f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.74f, 0.84f, 0.96f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.62f, 0.76f, 0.94f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.12f, 0.16f, 0.22f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.47f, 0.55f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.73f, 0.79f, 0.88f, 1.0f);

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
    request.start_node_id = rebuild_intent->start_node_id;
    (void)execute_feature_rebuild(request);
}

bool Application::execute_feature_rebuild(const model::RebuildRequest& request, const std::string& payload_override, bool allow_retry_popup) {
    auto delegate = [&](const nlohmann::json& payload) -> model::RebuildDelegateResult {
        const std::string transport_payload = payload_override.empty() ? payload.dump() : payload_override;
        const worker_result result = g_orchestrator.submit_geometry_request_once(transport_payload);
        if (result == worker_result::ok) {
            return {true, false, {}};
        }
        if (result == worker_result::crashed) {
            return {false, true, "GeometryWorker crashed during rebuild"};
        }
        return {false, false, "GeometryWorker failed rebuild request"};
    };

    const model::RebuildResult rebuild_result = feature_tree_.rebuild(
        request,
        delegate,
        [&]() {
            (void)g_orchestrator.submit_geometry_request_once("tessellate");
        },
        [&]() {
            (void)g_orchestrator.submit_geometry_request_once("upload_mesh");
        },
        [&]() {
            (void)g_orchestrator.submit_geometry_request_once("repaint");
        });

    if (rebuild_result.success) {
        return true;
    }

    if (rebuild_result.worker_crashed) {
        if (!allow_retry_popup) {
            if (rebuild_result.failed_node_id != 0U) {
                (void)feature_tree_.set_feature_state(rebuild_result.failed_node_id, model::FeatureState::Error);
            }
            return false;
        }

        worker_retry_context_ = WorkerRetryContext{};
        worker_retry_context_->request = model::RebuildRequest{false, rebuild_result.failed_node_id};
        worker_retry_context_->failed_node_id = rebuild_result.failed_node_id;

        nlohmann::json payload = {
            {"command", "rebuild_feature"},
            {"feature_id", rebuild_result.failed_node_id},
            {"full_rebuild", false},
            {"start_node_id", rebuild_result.failed_node_id},
        };
        const std::string serialized = payload.dump(2);
        std::snprintf(worker_retry_context_->payload_buffer.data(), worker_retry_context_->payload_buffer.size(), "%s", serialized.c_str());
        worker_retry_popup_opened_ = false;
        return false;
    }

    if (rebuild_result.failed_node_id != 0U) {
        (void)feature_tree_.set_feature_state(rebuild_result.failed_node_id, model::FeatureState::Error);
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

    ImGui::SetNextWindowSize(ImVec2(560.0f, 300.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Geometry Worker Crash", nullptr, ImGuiWindowFlags_NoResize)) {
        return;
    }

    ImGui::TextWrapped("GeometryWorker crashed while rebuilding feature %u. Review payload and decide whether to delegate again.", worker_retry_context_->failed_node_id);
    ImGui::Separator();
    ImGui::InputTextMultiline("Payload", worker_retry_context_->payload_buffer.data(), worker_retry_context_->payload_buffer.size(), ImVec2(-1.0f, 170.0f));

    if (ImGui::Button("Retry Delegation")) {
        const model::RebuildRequest retry_request = worker_retry_context_->request;
        const std::string retry_payload = std::string(worker_retry_context_->payload_buffer.data());

        worker_retry_context_.reset();
        worker_retry_popup_opened_ = false;
        ImGui::CloseCurrentPopup();

        if (g_orchestrator.restart_worker()) {
            (void)execute_feature_rebuild(retry_request, retry_payload, false);
        } else {
            last_error_ = "Failed to restart GeometryWorker";
            if (retry_request.start_node_id != 0U) {
                (void)feature_tree_.set_feature_state(retry_request.start_node_id, model::FeatureState::Error);
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel And Mark Error")) {
        const uint32_t failed_node_id = worker_retry_context_->failed_node_id;
        worker_retry_context_.reset();
        worker_retry_popup_opened_ = false;
        ImGui::CloseCurrentPopup();
        if (failed_node_id != 0U) {
            (void)feature_tree_.set_feature_state(failed_node_id, model::FeatureState::Error);
        }
    }

    ImGui::EndPopup();
}

void Application::draw_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        ImGui::MenuItem("New", "Ctrl+N");
        ImGui::MenuItem("Open", "Ctrl+O");
        ImGui::MenuItem("Save", "Ctrl+S");
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
