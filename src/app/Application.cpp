#include "app/Application.hpp"

#include <filesystem>
#include <fstream>
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

    bool submit_geometry_request(const std::string& payload) {
        const worker_result first_try = worker_.execute(payload);
        if (first_try == worker_result::ok) {
            return true;
        }

        if (first_try == worker_result::crashed) {
            worker_.stop();
            if (!worker_.start()) {
                return false;
            }

            const worker_result retry = worker_.execute(payload);
            return retry == worker_result::ok;
        }

        return false;
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

    if (io::CameraSession::load("session/camera.json", camera_)) {
        camera_session_loaded_ = true;
    }

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
            camera_.handle_event(event);
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

        if (!g_orchestrator.submit_geometry_request("frame_update_payload")) {
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
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.84f, 0.88f, 0.94f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.77f, 0.84f, 0.93f, 1.0f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.93f, 0.95f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.80f, 0.86f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.82f, 0.96f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.56f, 0.74f, 0.94f, 1.0f);
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
    viewport_panel_.draw();
    properties_panel_.draw();

    draw_status_bar();

    ImGui::Render();
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
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
        ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
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
    ImGui::Text("Features: 0");
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("Camera: %s", camera_.state().projection_mode == scene::ProjectionMode::perspective ? "perspective" : "orthographic");

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
