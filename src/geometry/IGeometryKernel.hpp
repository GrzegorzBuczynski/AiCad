#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <glm/glm.hpp>

#if defined(VULCANCAD_HAS_OCCT)
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#else
class gp_Dir;
class gp_Pnt;
#endif

namespace geometry {

using SolidHandle = uint32_t;

constexpr SolidHandle k_invalid_solid_handle = 0U;

/**
 * @brief 3D vertex used by tessellated mesh output.
 */
struct Vertex {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

/**
 * @brief CPU mesh container produced by geometry kernels.
 */
struct MeshData {
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};
};

/**
 * @brief Axis-aligned bounding box.
 */
struct AABB {
    glm::vec3 min{0.0f, 0.0f, 0.0f};
    glm::vec3 max{0.0f, 0.0f, 0.0f};
};

/**
 * @brief Polyline profile used by extrude/revolve constructors.
 */
struct Profile {
    std::vector<glm::vec2> points{};
    bool closed = true;
};

using EdgePolyline = std::vector<glm::vec3>;
using EdgePolylines = std::vector<EdgePolyline>;

using TessellationCallback = std::function<void(MeshData)>;

/**
 * @brief Abstract geometry kernel API for CAD solid operations.
 */
class IGeometryKernel {
public:
    /**
     * @brief Virtual destructor for polymorphic ownership.
     */
    virtual ~IGeometryKernel() = default;

    /**
     * @brief Creates a box primitive.
     * @param w Width.
     * @param h Height.
     * @param d Depth.
     * @return Opaque solid handle, or k_invalid_solid_handle on failure.
     */
    virtual SolidHandle createBox(float w, float h, float d) = 0;

    /**
     * @brief Creates a solid by extruding a profile.
     * @param profile Input profile.
     * @param d Extrusion distance.
     * @return Opaque solid handle, or k_invalid_solid_handle on failure.
     */
    virtual SolidHandle createExtrude(const Profile& profile, float d) = 0;

    /**
     * @brief Creates a solid by revolving a profile.
     * @param profile Input profile.
     * @param ang Revolution angle in degrees.
     * @return Opaque solid handle, or k_invalid_solid_handle on failure.
     */
    virtual SolidHandle createRevolve(const Profile& profile, float ang) = 0;

    /**
     * @brief Creates a union of two solids.
     * @param a First solid handle.
     * @param b Second solid handle.
     * @return Opaque solid handle, or k_invalid_solid_handle on failure.
     */
    virtual SolidHandle booleanUnion(SolidHandle a, SolidHandle b) = 0;

    /**
     * @brief Cuts the base solid using a tool solid.
     * @param base Base solid handle.
     * @param tool Tool solid handle.
     * @return Opaque solid handle, or k_invalid_solid_handle on failure.
     */
    virtual SolidHandle booleanCut(SolidHandle base, SolidHandle tool) = 0;

    /**
     * @brief Tessellates a solid into a triangle mesh.
     * @param solid Solid handle.
     * @param chord Chord tolerance.
     * @return Triangle mesh data. Empty on failure.
     */
    virtual MeshData tessellate(SolidHandle solid, float chord) = 0;

    /**
     * @brief Returns axis-aligned bounds for a solid.
     * @param solid Solid handle.
     * @return Solid bounds.
     */
    virtual AABB computeAABB(SolidHandle solid) = 0;

    /**
     * @brief Performs ray-based solid picking.
     * @param origin Ray origin in world space.
     * @param direction Ray direction in world space.
     * @return Hit solid handle, or k_invalid_solid_handle when no hit.
     */
    virtual SolidHandle pickSolid(const gp_Pnt& origin, const gp_Dir& direction) = 0;

    /**
     * @brief Returns discretized edge polylines for a solid.
     * @param solid Solid handle.
     * @return List of edge polylines, empty when unavailable.
     */
    virtual EdgePolylines getEdges(SolidHandle solid) = 0;

    /**
     * @brief Runs tessellation asynchronously on an internal thread pool.
     * @param solid Solid handle.
     * @param chord Chord tolerance.
     * @param callback Completion callback receiving mesh data.
     * @return True when job was accepted.
     */
    virtual bool tessellateAsync(SolidHandle solid, float chord, TessellationCallback callback) = 0;
};

}  // namespace geometry
