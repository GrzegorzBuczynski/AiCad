#include "app/GeometryIpc.hpp"

namespace app::ipc {

bool GeometryWorker::start() {
    running_ = true;
    return true;
}

void GeometryWorker::stop() {
    running_ = false;
}

GeometryResponse GeometryWorker::execute(const GeometryRequest& request) {
    if (!running_) {
        return {false, true, "GeometryWorker is not running"};
    }

    switch (request.command) {
    case GeometryCommand::FrameUpdate:
    case GeometryCommand::Tessellate:
    case GeometryCommand::UploadMesh:
    case GeometryCommand::Repaint:
        return {true, false, {}};

    case GeometryCommand::RebuildFromSketch:
        return {true, false, {}};

    case GeometryCommand::RebuildFeature:
        if (!request.full_rebuild && request.feature_id == 0U && request.start_feature_id == 0U) {
            return {false, false, "Missing feature_id/start_feature_id for partial rebuild"};
        }
        return {true, false, {}};
    }

    return {false, false, "Unsupported command"};
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
