#include "geometry/OcctKernel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <iostream>

#if defined(VULCANCAD_HAS_OCCT)
#include <BRepExtrema_DistShapeShape.hxx>
#include <Extrema_ExtFlag.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepIntCurveSurface_Inter.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#endif

namespace geometry {

OcctKernel::OcctKernel()
    : thread_pool_(std::max(1U, std::thread::hardware_concurrency())) {}

SolidHandle OcctKernel::createBox(float w, float h, float d) {
#if defined(VULCANCAD_HAS_OCCT)
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(
        std::max(0.0f, w),
        std::max(0.0f, h),
        std::max(0.0f, d)).Shape();
    return store_shape(box);
#else
    return fallback_.createBox(w, h, d);
#endif
}

SolidHandle OcctKernel::createExtrude(const Profile& profile, float d) {
#if defined(VULCANCAD_HAS_OCCT)
    if (profile.points.size() < 3U) {
        return k_invalid_solid_handle;
    }

    BRepBuilderAPI_MakePolygon polygon_builder;
    for (const glm::vec2& point : profile.points) {
        polygon_builder.Add(gp_Pnt(point.x, point.y, 0.0));
    }
    if (profile.closed) {
        polygon_builder.Close();
    }

    if (!polygon_builder.IsDone()) {
        return k_invalid_solid_handle;
    }

    const TopoDS_Wire wire = polygon_builder.Wire();
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(wire).Face();
    const TopoDS_Shape prism = BRepPrimAPI_MakePrism(face, gp_Vec(0.0, 0.0, d)).Shape();
    return store_shape(prism);
#else
    return fallback_.createExtrude(profile, d);
#endif
}

SolidHandle OcctKernel::createRevolve(const Profile& profile, float ang) {
#if defined(VULCANCAD_HAS_OCCT)
    if (profile.points.size() < 3U) {
        return k_invalid_solid_handle;
    }

    BRepBuilderAPI_MakePolygon polygon_builder;
    for (const glm::vec2& point : profile.points) {
        polygon_builder.Add(gp_Pnt(point.x, point.y, 0.0));
    }
    if (profile.closed) {
        polygon_builder.Close();
    }

    if (!polygon_builder.IsDone()) {
        return k_invalid_solid_handle;
    }

    const TopoDS_Wire wire = polygon_builder.Wire();
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(wire).Face();
    const double radians = static_cast<double>(ang) * 3.14159265358979323846 / 180.0;
    const gp_Ax1 axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0));
    const TopoDS_Shape revolved = BRepPrimAPI_MakeRevol(face, axis, radians, true).Shape();
    return store_shape(revolved);
#else
    return fallback_.createRevolve(profile, ang);
#endif
}

SolidHandle OcctKernel::createEdge(const glm::vec3& a, const glm::vec3& b) {
#if defined(VULCANCAD_HAS_OCCT)
    const gp_Pnt a_point(a.x, a.y, a.z);
    const gp_Pnt b_point(b.x, b.y, b.z);
    const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(a_point, b_point).Edge();
    return store_shape(edge);
#else
    return fallback_.createEdge(a, b);
#endif
}

SolidHandle OcctKernel::booleanUnion(SolidHandle a, SolidHandle b) {
#if defined(VULCANCAD_HAS_OCCT)
    TopoDS_Shape a_shape{};
    TopoDS_Shape b_shape{};
    if (!get_shape(a, &a_shape) || !get_shape(b, &b_shape)) {
        return k_invalid_solid_handle;
    }

    BRepAlgoAPI_Fuse fuse(a_shape, b_shape);
    if (!fuse.IsDone()) {
        return k_invalid_solid_handle;
    }

    return store_shape(fuse.Shape());
#else
    return fallback_.booleanUnion(a, b);
#endif
}

SolidHandle OcctKernel::booleanCut(SolidHandle base, SolidHandle tool) {
#if defined(VULCANCAD_HAS_OCCT)
    TopoDS_Shape base_shape{};
    TopoDS_Shape tool_shape{};
    if (!get_shape(base, &base_shape) || !get_shape(tool, &tool_shape)) {
        return k_invalid_solid_handle;
    }

    BRepAlgoAPI_Cut cut(base_shape, tool_shape);
    if (!cut.IsDone()) {
        return k_invalid_solid_handle;
    }

    return store_shape(cut.Shape());
#else
    return fallback_.booleanCut(base, tool);
#endif
}

MeshData OcctKernel::tessellate(SolidHandle solid, float chord) {
#if defined(VULCANCAD_HAS_OCCT)
    TopoDS_Shape shape{};
    if (!get_shape(solid, &shape)) {
        return {};
    }

    const double safe_chord = std::max(static_cast<double>(chord), 0.001);
    BRepMesh_IncrementalMesh mesher(shape, safe_chord, false, 0.5, true);
    if (!mesher.IsDone()) {
        return {};
    }

    MeshData mesh{};
    TopExp_Explorer explorer(shape, TopAbs_FACE);
    while (explorer.More()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        TopLoc_Location location;
        const Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) {
            explorer.Next();
            continue;
        }

        const gp_Trsf transform = location.Transformation();
        const int node_count = triangulation->NbNodes();
        const int triangle_count = triangulation->NbTriangles();

        const uint32_t base_vertex = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.reserve(mesh.vertices.size() + static_cast<size_t>(node_count));

        for (int i = 1; i <= node_count; ++i) {
            gp_Pnt point = triangulation->Node(i);
            point.Transform(transform);
            mesh.vertices.push_back({
                {static_cast<float>(point.X()), static_cast<float>(point.Y()), static_cast<float>(point.Z())},
                {0.0f, 1.0f, 0.0f}});
        }

        mesh.indices.reserve(mesh.indices.size() + static_cast<size_t>(triangle_count) * 3U);
        for (int i = 1; i <= triangle_count; ++i) {
            int i1 = 0;
            int i2 = 0;
            int i3 = 0;
            triangulation->Triangle(i).Get(i1, i2, i3);
            mesh.indices.push_back(base_vertex + static_cast<uint32_t>(i1 - 1));
            mesh.indices.push_back(base_vertex + static_cast<uint32_t>(i2 - 1));
            mesh.indices.push_back(base_vertex + static_cast<uint32_t>(i3 - 1));
        }

        explorer.Next();
    }

    return mesh;
#else
    return fallback_.tessellate(solid, chord);
#endif
}

AABB OcctKernel::computeAABB(SolidHandle solid) {
#if defined(VULCANCAD_HAS_OCCT)
    TopoDS_Shape shape{};
    if (!get_shape(solid, &shape)) {
        return {};
    }

    Bnd_Box box;
    BRepBndLib::Add(shape, box);

    Standard_Real min_x = 0.0;
    Standard_Real min_y = 0.0;
    Standard_Real min_z = 0.0;
    Standard_Real max_x = 0.0;
    Standard_Real max_y = 0.0;
    Standard_Real max_z = 0.0;
    box.Get(min_x, min_y, min_z, max_x, max_y, max_z);

    return {
        {static_cast<float>(min_x), static_cast<float>(min_y), static_cast<float>(min_z)},
        {static_cast<float>(max_x), static_cast<float>(max_y), static_cast<float>(max_z)},
    };
#else
    return fallback_.computeAABB(solid);
#endif
}

SolidHandle OcctKernel::pickSolid(const gp_Pnt& origin, const gp_Dir& direction, double edge_tolerance_world) {
#if defined(VULCANCAD_HAS_OCCT)
    std::cout << "[PICK] pickSolid called - origin: (" << origin.X() << ", " << origin.Y() << ", " << origin.Z()
              << "), dir: (" << direction.X() << ", " << direction.Y() << ", " << direction.Z()
              << "), edge_tolerance: " << edge_tolerance_world << " mm" << std::endl;

    const gp_Lin ray(origin, direction);
    const gp_Pnt ray_end(
        origin.X() + direction.X() * 10000.0,
        origin.Y() + direction.Y() * 10000.0,
        origin.Z() + direction.Z() * 10000.0);
    const TopoDS_Edge ray_edge = BRepBuilderAPI_MakeEdge(origin, ray_end).Edge();
    constexpr Standard_Real tolerance = 1.0e-6;
    const Standard_Real edge_tolerance = std::max<Standard_Real>(
        static_cast<Standard_Real>(edge_tolerance_world),
        1.0e-4);

    std::cout << "[PICK] Computed edge_tolerance: " << edge_tolerance << std::endl;

    // For edge picking: track the closest edge by distance
    Standard_Real nearest_edge_distance = std::numeric_limits<Standard_Real>::max();
    SolidHandle nearest_edge_handle = k_invalid_solid_handle;

    // For solid picking: track by w parameter along ray
    Standard_Real nearest_w = std::numeric_limits<Standard_Real>::max();
    SolidHandle nearest_solid_handle = k_invalid_solid_handle;

    std::scoped_lock lock(shapes_mutex_);
    std::cout << "[PICK] Total shapes in map: " << shapes_.size() << std::endl;

    for (const auto& [handle, shape] : shapes_) {
        if (handle == k_invalid_solid_handle || shape.IsNull()) {
            continue;
        }

        const TopAbs_ShapeEnum shape_type = shape.ShapeType();
        std::cout << "[PICK] Checking handle=" << handle << ", shape_type=" << shape_type << std::endl;

        // Check if shape is an edge or wire (sketch geometry) - pick by minimum distance
        if (shape_type == TopAbs_EDGE || shape_type == TopAbs_WIRE) {
            BRepExtrema_DistShapeShape distCalc(shape, ray_edge, Extrema_ExtFlag_MIN);
            distCalc.Perform();
            std::cout << "[PICK] Edge/Wire distance check: handle=" << handle
                      << ", is_done=" << distCalc.IsDone()
                      << ", nb_solutions=" << distCalc.NbSolution();
            if (distCalc.IsDone() && distCalc.NbSolution() > 0) {
                std::cout << ", distance=" << distCalc.Value();
            }
            std::cout << std::endl;

            if (distCalc.IsDone() && distCalc.Value() < edge_tolerance) {
                std::cout << "[PICK] Edge/Wire HIT! handle=" << handle << ", distance=" << distCalc.Value() << std::endl;
                if (distCalc.Value() < nearest_edge_distance) {
                    nearest_edge_distance = distCalc.Value();
                    nearest_edge_handle = handle;
                }
            }
            continue;
        }

        // For solid shapes, try face intersection first
        BRepIntCurveSurface_Inter intersector;
        intersector.Init(shape, ray, tolerance);
        bool shape_hit = false;
        while (intersector.More()) {
            const Standard_Real w = intersector.W();
            if (w >= 0.0 && w < nearest_w) {
                nearest_w = w;
                nearest_solid_handle = handle;
                shape_hit = true;
            }
            intersector.Next();
        }

        if (shape_hit) {
            std::cout << "[PICK] Face HIT! handle=" << handle << ", w=" << nearest_w << std::endl;
        }

        // If no face hit, check edges of composite shape
        if (!shape_hit) {
            TopExp_Explorer edge_explorer(shape, TopAbs_EDGE);
            int edge_count = 0;
            while (edge_explorer.More()) {
                const TopoDS_Edge edge = TopoDS::Edge(edge_explorer.Current());
                BRepExtrema_DistShapeShape distCalc(edge, ray_edge, Extrema_ExtFlag_MIN);
                distCalc.Perform();
                if (distCalc.IsDone() && distCalc.NbSolution() > 0) {
                    std::cout << "[PICK] Solid edge distance: handle=" << handle
                              << ", edge_idx=" << edge_count
                              << ", distance=" << distCalc.Value() << std::endl;
                }
                if (distCalc.IsDone() && distCalc.Value() < edge_tolerance) {
                    const gp_Pnt hit_point = distCalc.PointOnShape2(1);
                    const gp_Vec to_hit(origin, hit_point);
                    const Standard_Real w = to_hit.Dot(gp_Vec(direction));
                    if (w >= 0.0 && w < nearest_w) {
                        nearest_w = w;
                        nearest_solid_handle = handle;
                        std::cout << "[PICK] Solid edge HIT! handle=" << handle << ", w=" << w << std::endl;
                        break;
                    }
                }
                edge_explorer.Next();
                ++edge_count;
            }
        }
    }

    // Prefer edge hit (closest by distance) over solid hit (closest by w)
    if (nearest_edge_handle != k_invalid_solid_handle) {
        std::cout << "[PICK] Final result: handle=" << nearest_edge_handle << " (edge, distance=" << nearest_edge_distance << ")" << std::endl;
        return nearest_edge_handle;
    }
    std::cout << "[PICK] Final result: handle=" << nearest_solid_handle << " (solid, w=" << nearest_w << ")" << std::endl;
    return nearest_solid_handle;
#else
    return fallback_.pickSolid(origin, direction, edge_tolerance_world);
#endif
}

EdgePolylines OcctKernel::getEdges(SolidHandle solid) {
#if defined(VULCANCAD_HAS_OCCT)
    TopoDS_Shape shape{};
    if (!get_shape(solid, &shape)) {
        return {};
    }

    EdgePolylines edges{};
    TopExp_Explorer explorer(shape, TopAbs_EDGE);
    while (explorer.More()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        BRepAdaptor_Curve curve(edge);
        const Standard_Real first = curve.FirstParameter();
        const Standard_Real last = curve.LastParameter();

        if (!std::isfinite(first) || !std::isfinite(last)) {
            explorer.Next();
            continue;
        }

        constexpr int samples = 24;
        EdgePolyline polyline{};
        polyline.reserve(samples + 1);

        if (std::abs(last - first) < 1.0e-9) {
            const gp_Pnt point = curve.Value(first);
            polyline.push_back({
                static_cast<float>(point.X()),
                static_cast<float>(point.Y()),
                static_cast<float>(point.Z()),
            });
        } else {
            for (int i = 0; i <= samples; ++i) {
                const Standard_Real t = static_cast<Standard_Real>(i) / static_cast<Standard_Real>(samples);
                const Standard_Real parameter = first + (last - first) * t;
                const gp_Pnt point = curve.Value(parameter);
                polyline.push_back({
                    static_cast<float>(point.X()),
                    static_cast<float>(point.Y()),
                    static_cast<float>(point.Z()),
                });
            }
        }

        if (polyline.size() >= 2U) {
            edges.push_back(std::move(polyline));
        }

        explorer.Next();
    }

    return edges;
#else
    return fallback_.getEdges(solid);
#endif
}

bool OcctKernel::tessellateAsync(SolidHandle solid, float chord, TessellationCallback callback) {
    if (!callback) {
        return false;
    }

    return thread_pool_.enqueue([this, solid, chord, callback = std::move(callback)]() mutable {
        callback(tessellate(solid, chord));
    });
}

#if defined(VULCANCAD_HAS_OCCT)
SolidHandle OcctKernel::store_shape(const TopoDS_Shape& shape) {
    const SolidHandle handle = next_handle_.fetch_add(1U);
    if (handle == k_invalid_solid_handle) {
        return k_invalid_solid_handle;
    }

    std::scoped_lock lock(shapes_mutex_);
    shapes_[handle] = shape;
    return handle;
}

bool OcctKernel::get_shape(SolidHandle handle, TopoDS_Shape* out_shape) const {
    if (out_shape == nullptr || handle == k_invalid_solid_handle) {
        return false;
    }

    std::scoped_lock lock(shapes_mutex_);
    const auto it = shapes_.find(handle);
    if (it == shapes_.end()) {
        return false;
    }

    *out_shape = it->second;
    return true;
}
#endif

}  // namespace geometry
