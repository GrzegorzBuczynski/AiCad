#pragma once

#include <atomic>
#include <unordered_map>
#include <mutex>

#include "geometry/GeometryThreadPool.hpp"
#include "geometry/IGeometryKernel.hpp"

namespace geometry {

/**
 * @brief Analytic fallback kernel with box-based approximations.
 */
class FallbackKernel final : public IGeometryKernel {
public:
    /**
     * @brief Constructs fallback kernel.
     */
    FallbackKernel();

    /**
     * @brief Destroys fallback kernel and worker pool.
     */
    ~FallbackKernel() override = default;

    SolidHandle createBox(float w, float h, float d) override;
    SolidHandle createExtrude(const Profile& profile, float d) override;
    SolidHandle createRevolve(const Profile& profile, float ang) override;
    SolidHandle booleanUnion(SolidHandle a, SolidHandle b) override;
    SolidHandle booleanCut(SolidHandle base, SolidHandle tool) override;
    MeshData tessellate(SolidHandle solid, float chord) override;
    AABB computeAABB(SolidHandle solid) override;
    bool tessellateAsync(SolidHandle solid, float chord, TessellationCallback callback) override;

private:
    struct SolidRecord {
        AABB bounds{};
    };

    SolidHandle create_record(const AABB& bounds);
    bool get_record(SolidHandle solid, SolidRecord* out_record) const;
    MeshData tessellate_box(const AABB& bounds, float chord) const;

    mutable std::mutex solids_mutex_{};
    std::unordered_map<SolidHandle, SolidRecord> solids_{};
    std::atomic<uint32_t> next_handle_{1U};
    GeometryThreadPool thread_pool_;
};

}  // namespace geometry
