#pragma once

#include <cstdint>
#include <string>

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
    Repaint
};

/**
 * @brief Typed rebuild/runtime request sent from module 1 to module 2.
 */
struct GeometryRequest {
    GeometryCommand command = GeometryCommand::FrameUpdate;
    uint32_t feature_id = 0U;
    bool full_rebuild = false;
    uint32_t start_feature_id = 0U;
};

/**
 * @brief Typed response returned by module 2.
 */
struct GeometryResponse {
    bool success = false;
    bool worker_crashed = false;
    std::string message{};
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
