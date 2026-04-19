cmake_minimum_required(VERSION 3.28)
project(VulcanCAD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_DX12 "Enable optional DirectX 12 backend" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerFlags)

find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OCCT QUIET)

add_executable(VulcanCAD
    src/main.cpp
    src/app/Application.cpp
    src/app/GeometryIpc.cpp
    src/app/Window.cpp
    src/renderer/VulkanContext.cpp
    src/renderer/RenderFrame.cpp
    src/scene/Camera.cpp
    src/io/CameraSession.cpp
    src/io/ModelSerializer.cpp
    src/io/ModelDeserializer.cpp
    src/model/FeatureTree.cpp
    src/sketch/SketchEntity.cpp
    src/sketch/SketchSolver.cpp
    src/sketch/SketchDocument.cpp
    src/sketch/SketchView.cpp
    src/geometry/GeometryThreadPool.cpp
    src/geometry/FallbackKernel.cpp
    src/geometry/OcctKernel.cpp
    src/git/LocalRepoManager.cpp
    src/ui/FeatureTreePanel.cpp
    src/ui/ViewportPanel.cpp
    src/ui/PropertiesPanel.cpp
)

target_include_directories(VulcanCAD PRIVATE src)

target_link_libraries(VulcanCAD
    PRIVATE
        Vulkan::Vulkan
        SDL3::SDL3
        glm::glm
        nlohmann_json::nlohmann_json
)

if(WIN32)
    add_custom_command(TARGET VulcanCAD POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:VulcanCAD>
            $<TARGET_FILE_DIR:VulcanCAD>
        COMMAND_EXPAND_LISTS
    )

    if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/bin/vulkan-1.dll")
        add_custom_command(TARGET VulcanCAD POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/bin/vulkan-1.dll"
                "$<TARGET_FILE_DIR:VulcanCAD>/vulkan-1.dll"
        )
    endif()
endif()

add_custom_command(TARGET VulcanCAD POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/settings.json
        $<TARGET_FILE_DIR:VulcanCAD>/settings.json
)

if(TARGET imgui::imgui)
    target_link_libraries(VulcanCAD PRIVATE imgui::imgui)
elseif(TARGET imgui)
    target_link_libraries(VulcanCAD PRIVATE imgui)
else()
    message(FATAL_ERROR "ImGui package found, but no known target (imgui::imgui or imgui).")
endif()

if(OCCT_FOUND)
    target_compile_definitions(VulcanCAD PRIVATE VULCANCAD_HAS_OCCT=1)
    if(DEFINED OCCT_INCLUDE_DIRS)
        target_include_directories(VulcanCAD PRIVATE ${OCCT_INCLUDE_DIRS})
    endif()
    if(DEFINED OCCT_LIBRARIES)
        target_link_libraries(VulcanCAD PRIVATE ${OCCT_LIBRARIES})
    endif()
endif()

if(USE_DX12)
    target_compile_definitions(VulcanCAD PRIVATE USE_DX12=1)
endif()

apply_default_compiler_flags(VulcanCAD)

enable_testing()
add_subdirectory(tests)
