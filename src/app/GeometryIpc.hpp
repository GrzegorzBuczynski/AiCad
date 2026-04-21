#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "geometry/IGeometryKernel.hpp"
#include "geometry/OcctKernel.hpp"

namespace app::ipc {

/**
 * @brief GeometryWorker command set used by typed IPC contract.
 */
enum class GeometryCommand {
    FrameUpdate,
    RebuildFeature,
    RebuildFromSketch,
    Tessellate,
    UploadMesh,
    Repaint,
    PickSolid
};

/**
 * @brief Typed rebuild/runtime request sent from module 1 to module 2.
 */
struct GeometryRequest {
    GeometryCommand command = GeometryCommand::FrameUpdate;
    uint32_t feature_id = 0U;
    bool full_rebuild = false;
    uint32_t start_feature_id = 0U;
    double ray_origin_x = 0.0;
    double ray_origin_y = 0.0;
    double ray_origin_z = 0.0;
    double ray_dir_x = 0.0;
    double ray_dir_y = 0.0;
    double ray_dir_z = 1.0;
};

/**
 * @brief Typed response returned by module 2.
 */
struct GeometryResponse {
    bool success = false;
    bool worker_crashed = false;
    std::string message{};
    uint32_t hit_solid_handle = 0U;
    uint32_t hit_feature_id = 0U;
    geometry::EdgePolylines hit_edges{};
};

/**
 * @brief Process-isolated GeometryWorker facade.
 */
class GeometryWorker {
public:
    /**
     * @brief Starts worker runtime.
     * @return True on success.
     */
    bool start();

    /**
     * @brief Stops worker runtime.
     */
    void stop();

    /**
     * @brief Executes a typed request in worker.
     * @param request Request payload.
     * @return Execution result.
     */
    GeometryResponse execute(const GeometryRequest& request);

private:
    bool running_ = false;
    geometry::OcctKernel kernel_{};
    std::unordered_map<uint32_t, geometry::SolidHandle> feature_solids_{};
    std::unordered_map<geometry::SolidHandle, uint32_t> solid_features_{};
};

/**
 * @brief Module 1 orchestrator-side IPC adapter.
 */
class MainOrchestrator {
public:
    /**
     * @brief Initializes worker module.
     * @return True on success.
     */
    bool init();

    /**
     * @brief Shuts down worker module.
     */
    void shutdown();

    /**
     * @brief Sends single request to worker without implicit retry.
     * @param request Typed request.
     * @return Worker response.
     */
    GeometryResponse submit_once(const GeometryRequest& request);

    /**
     * @brief Restarts worker module after crash.
     * @return True when worker restarted successfully.
     */
    bool restart_worker();

private:
    GeometryWorker worker_{};
};

}  // namespace app::ipc
