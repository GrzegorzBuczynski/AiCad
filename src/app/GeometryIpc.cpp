#include "app/GeometryIpc.hpp"

#include <iostream>
#include <unordered_map>

#if defined(VULCANCAD_HAS_OCCT)
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#endif

#include "geometry/OcctKernel.hpp"

namespace app::ipc {

namespace {

geometry::SolidHandle build_feature_proxy_solid(geometry::OcctKernel& kernel, uint32_t feature_id) {
    const float base = 0.6f + static_cast<float>(feature_id % 7U) * 0.12f;
    return kernel.createBox(base, base, base);
}

}  // namespace

bool GeometryWorker::start() {
    running_ = true;
    feature_solids_.clear();
    solid_features_.clear();
    return true;
}

void GeometryWorker::stop() {
    running_ = false;
    feature_solids_.clear();
    solid_features_.clear();
}

GeometryResponse GeometryWorker::execute(const GeometryRequest& request) {
    if (!running_) {
        return {false, true, "GeometryWorker is not running", 0U, 0U, {}};
    }

    switch (request.command) {
    case GeometryCommand::FrameUpdate:
    case GeometryCommand::Tessellate:
    case GeometryCommand::UploadMesh:
    case GeometryCommand::Repaint:
        return {true, false, {}, 0U, 0U, {}};

    case GeometryCommand::RebuildFromSketch:
        std::cout << "[IPC] RebuildFromSketch: " << request.sketch_segments.size() << " segments" << std::endl;
        for (const SketchSegment& segment : request.sketch_segments) {
            const glm::vec3 a{
                static_cast<float>(segment.ax),
                static_cast<float>(segment.ay),
                static_cast<float>(segment.az),
            };
            const glm::vec3 b{
                static_cast<float>(segment.bx),
                static_cast<float>(segment.by),
                static_cast<float>(segment.bz),
            };

            const geometry::SolidHandle edge_handle = kernel_.createEdge(a, b);
            if (edge_handle == geometry::k_invalid_solid_handle) {
                std::cout << "[IPC] createEdge failed for segment (" << segment.ax << "," << segment.ay << "," << segment.az
                          << ") -> (" << segment.bx << "," << segment.by << "," << segment.bz << ")" << std::endl;
                continue;
            }
            solid_features_[edge_handle] = 0U;
            std::cout << "[IPC] Created edge handle=" << edge_handle << ", solid_features_ size=" << solid_features_.size() << std::endl;
        }
        return {true, false, {}, 0U, 0U, {}};

    case GeometryCommand::RebuildFeature: {
        if (!request.full_rebuild && request.feature_id == 0U && request.start_feature_id == 0U) {
            return {false, false, "Missing feature_id/start_feature_id for partial rebuild", 0U, 0U, {}};
        }

        if (request.full_rebuild) {
            feature_solids_.clear();
            solid_features_.clear();
        }

        if (request.feature_id != 0U) {
            const geometry::SolidHandle solid = build_feature_proxy_solid(kernel_, request.feature_id);
            if (solid == geometry::k_invalid_solid_handle) {
                return {false, false, "Failed to create proxy solid for feature", 0U, 0U, {}};
            }
            feature_solids_[request.feature_id] = solid;
            solid_features_[solid] = request.feature_id;
        }

        return {true, false, {}, 0U, 0U, {}};
    }

    case GeometryCommand::PickSolid: {
#if defined(VULCANCAD_HAS_OCCT)
        std::cout << "[IPC] PickSolid: ray_origin=(" << request.ray_origin_x << ", " << request.ray_origin_y << ", "
                  << request.ray_origin_z << "), ray_dir=(" << request.ray_dir_x << ", " << request.ray_dir_y << ", "
                  << request.ray_dir_z << "), edge_tolerance=" << request.edge_tolerance_mm << std::endl;
        std::cout << "[IPC] solid_features_ map size: " << solid_features_.size() << std::endl;

        const gp_Pnt origin(request.ray_origin_x, request.ray_origin_y, request.ray_origin_z);
        gp_Dir direction(0.0, 0.0, 1.0);
        try {
            direction = gp_Dir(request.ray_dir_x, request.ray_dir_y, request.ray_dir_z);
        } catch (...) {
            std::cout << "[IPC] Invalid ray direction exception" << std::endl;
            return {false, false, "Invalid ray direction", 0U, 0U, {}};
        }

        const geometry::SolidHandle hit = kernel_.pickSolid(origin, direction, request.edge_tolerance_mm);
        std::cout << "[IPC] pickSolid returned handle: " << hit << std::endl;

        if (hit == geometry::k_invalid_solid_handle) {
            std::cout << "[IPC] No hit - returning empty response" << std::endl;
            return {true, false, {}, 0U, 0U, {}};
        }

        const auto it = solid_features_.find(hit);
        const uint32_t hit_feature_id = it != solid_features_.end() ? it->second : 0U;
        std::cout << "[IPC] hit_feature_id=" << hit_feature_id << " (from solid_features_ map)" << std::endl;

        const auto edges = kernel_.getEdges(hit);
        std::cout << "[IPC] getEdges returned " << edges.size() << " edge polylines" << std::endl;
        return {true, false, {}, hit, hit_feature_id, edges};
#else
    return {true, false, "OCCT picker unavailable in this build", 0U, 0U, {}};
#endif
    }
    }

    return {false, false, "Unsupported command", 0U, 0U, {}};
}

bool MainOrchestrator::init() {
    return worker_.start();
}

void MainOrchestrator::shutdown() {
    worker_.stop();
}

GeometryResponse MainOrchestrator::submit_once(const GeometryRequest& request) {
    return worker_.execute(request);
}

bool MainOrchestrator::restart_worker() {
    worker_.stop();
    return worker_.start();
}

}  // namespace app::ipc
