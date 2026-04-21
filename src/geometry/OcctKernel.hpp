#pragma once

#include "geometry/FallbackKernel.hpp"
#include "geometry/GeometryThreadPool.hpp"
#include "geometry/IGeometryKernel.hpp"

#if defined(VULCANCAD_HAS_OCCT)
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Shape.hxx>
#endif

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace geometry {

/**
 * @brief Open CASCADE based geometry kernel.
 */
class OcctKernel final : public IGeometryKernel {
public:
    /**
     * @brief Constructs OCCT kernel.
     */
    OcctKernel();

    /**
     * @brief Destroys OCCT kernel and worker pool.
     */
    ~OcctKernel() override = default;

    SolidHandle createBox(float w, float h, float d) override;
    SolidHandle createExtrude(const Profile& profile, float d) override;
    SolidHandle createRevolve(const Profile& profile, float ang) override;
    SolidHandle createEdge(const glm::vec3& a, const glm::vec3& b) override;
    SolidHandle booleanUnion(SolidHandle a, SolidHandle b) override;
    SolidHandle booleanCut(SolidHandle base, SolidHandle tool) override;
    MeshData tessellate(SolidHandle solid, float chord) override;
    AABB computeAABB(SolidHandle solid) override;
    SolidHandle pickSolid(const gp_Pnt& origin, const gp_Dir& direction, double edge_tolerance_mm) override;
    EdgePolylines getEdges(SolidHandle solid) override;
    bool tessellateAsync(SolidHandle solid, float chord, TessellationCallback callback) override;

private:
#if defined(VULCANCAD_HAS_OCCT)
    SolidHandle store_shape(const TopoDS_Shape& shape);
    bool get_shape(SolidHandle handle, TopoDS_Shape* out_shape) const;

    mutable std::mutex shapes_mutex_{};
    std::unordered_map<SolidHandle, TopoDS_Shape> shapes_{};
    std::atomic<uint32_t> next_handle_{1U};
#endif

    GeometryThreadPool thread_pool_;
    FallbackKernel fallback_;
};

}  // namespace geometry
