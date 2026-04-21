#include "geometry/FallbackKernel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <thread>

namespace geometry {

namespace {

AABB make_box_bounds(float w, float h, float d) {
    const float half_w = std::max(w, 0.0f) * 0.5f;
    const float half_h = std::max(h, 0.0f) * 0.5f;
    const float half_d = std::max(d, 0.0f) * 0.5f;
    return {{-half_w, -half_h, -half_d}, {half_w, half_h, half_d}};
}

AABB merge_bounds(const AABB& a, const AABB& b) {
    return {
        {
            std::min(a.min.x, b.min.x),
            std::min(a.min.y, b.min.y),
            std::min(a.min.z, b.min.z),
        },
        {
            std::max(a.max.x, b.max.x),
            std::max(a.max.y, b.max.y),
            std::max(a.max.z, b.max.z),
        },
    };
}

AABB shrink_by_tool(const AABB& base, const AABB& tool) {
    const AABB overlap{
        {
            std::max(base.min.x, tool.min.x),
            std::max(base.min.y, tool.min.y),
            std::max(base.min.z, tool.min.z),
        },
        {
            std::min(base.max.x, tool.max.x),
            std::min(base.max.y, tool.max.y),
            std::min(base.max.z, tool.max.z),
        },
    };

    if (overlap.min.x >= overlap.max.x || overlap.min.y >= overlap.max.y || overlap.min.z >= overlap.max.z) {
        return base;
    }

    AABB out = base;
    out.max.x = std::max(base.min.x, base.max.x - (overlap.max.x - overlap.min.x) * 0.5f);
    out.max.y = std::max(base.min.y, base.max.y - (overlap.max.y - overlap.min.y) * 0.5f);
    out.max.z = std::max(base.min.z, base.max.z - (overlap.max.z - overlap.min.z) * 0.5f);
    return out;
}

}  // namespace

FallbackKernel::FallbackKernel()
    : thread_pool_(std::max(1U, std::thread::hardware_concurrency())) {}

SolidHandle FallbackKernel::createBox(float w, float h, float d) {
    return create_record(make_box_bounds(w, h, d));
}

SolidHandle FallbackKernel::createExtrude(const Profile& profile, float d) {
    if (profile.points.empty()) {
        return k_invalid_solid_handle;
    }

    glm::vec2 min_point = profile.points.front();
    glm::vec2 max_point = profile.points.front();
    for (const glm::vec2& point : profile.points) {
        min_point.x = std::min(min_point.x, point.x);
        min_point.y = std::min(min_point.y, point.y);
        max_point.x = std::max(max_point.x, point.x);
        max_point.y = std::max(max_point.y, point.y);
    }

    const float depth = std::max(d, 0.0f);
    const AABB bounds{{min_point.x, min_point.y, 0.0f}, {max_point.x, max_point.y, depth}};
    return create_record(bounds);
}

SolidHandle FallbackKernel::createRevolve(const Profile& profile, float ang) {
    if (profile.points.empty()) {
        return k_invalid_solid_handle;
    }

    float max_radius = 0.0f;
    float min_y = profile.points.front().y;
    float max_y = profile.points.front().y;
    for (const glm::vec2& point : profile.points) {
        max_radius = std::max(max_radius, std::abs(point.x));
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }

    const float clamped_angle = std::clamp(ang, 0.0f, 360.0f);
    const float scale = std::max(clamped_angle / 360.0f, 0.1f);
    const AABB bounds{{-max_radius * scale, min_y, -max_radius * scale}, {max_radius * scale, max_y, max_radius * scale}};
    return create_record(bounds);
}

SolidHandle FallbackKernel::booleanUnion(SolidHandle a, SolidHandle b) {
    SolidRecord first{};
    SolidRecord second{};
    if (!get_record(a, &first) || !get_record(b, &second)) {
        return k_invalid_solid_handle;
    }

    return create_record(merge_bounds(first.bounds, second.bounds));
}

SolidHandle FallbackKernel::booleanCut(SolidHandle base, SolidHandle tool) {
    SolidRecord base_record{};
    SolidRecord tool_record{};
    if (!get_record(base, &base_record) || !get_record(tool, &tool_record)) {
        return k_invalid_solid_handle;
    }

    return create_record(shrink_by_tool(base_record.bounds, tool_record.bounds));
}

MeshData FallbackKernel::tessellate(SolidHandle solid, float chord) {
    SolidRecord record{};
    if (!get_record(solid, &record)) {
        return {};
    }

    return tessellate_box(record.bounds, chord);
}

AABB FallbackKernel::computeAABB(SolidHandle solid) {
    SolidRecord record{};
    if (!get_record(solid, &record)) {
        return {};
    }

    return record.bounds;
}

SolidHandle FallbackKernel::pickSolid(const gp_Pnt& origin, const gp_Dir& direction) {
    (void)origin;
    (void)direction;
    return k_invalid_solid_handle;
}

EdgePolylines FallbackKernel::getEdges(SolidHandle solid) {
    (void)solid;
    return {};
}

bool FallbackKernel::tessellateAsync(SolidHandle solid, float chord, TessellationCallback callback) {
    if (!callback) {
        return false;
    }

    return thread_pool_.enqueue([this, solid, chord, callback = std::move(callback)]() mutable {
        callback(tessellate(solid, chord));
    });
}

SolidHandle FallbackKernel::create_record(const AABB& bounds) {
    const SolidHandle new_handle = next_handle_.fetch_add(1U);
    if (new_handle == k_invalid_solid_handle) {
        return k_invalid_solid_handle;
    }

    std::scoped_lock lock(solids_mutex_);
    solids_[new_handle] = SolidRecord{bounds};
    return new_handle;
}

bool FallbackKernel::get_record(SolidHandle solid, SolidRecord* out_record) const {
    if (out_record == nullptr || solid == k_invalid_solid_handle) {
        return false;
    }

    std::scoped_lock lock(solids_mutex_);
    const auto it = solids_.find(solid);
    if (it == solids_.end()) {
        return false;
    }

    *out_record = it->second;
    return true;
}

MeshData FallbackKernel::tessellate_box(const AABB& bounds, float chord) const {
    MeshData mesh{};

    const glm::vec3 min = bounds.min;
    const glm::vec3 max = bounds.max;
    const std::array<glm::vec3, 8> corners = {
        glm::vec3{min.x, min.y, min.z},
        glm::vec3{max.x, min.y, min.z},
        glm::vec3{max.x, max.y, min.z},
        glm::vec3{min.x, max.y, min.z},
        glm::vec3{min.x, min.y, max.z},
        glm::vec3{max.x, min.y, max.z},
        glm::vec3{max.x, max.y, max.z},
        glm::vec3{min.x, max.y, max.z},
    };

    mesh.vertices = {
        {corners[0], {0.0f, 0.0f, -1.0f}},
        {corners[1], {0.0f, 0.0f, -1.0f}},
        {corners[2], {0.0f, 0.0f, -1.0f}},
        {corners[3], {0.0f, 0.0f, -1.0f}},
        {corners[4], {0.0f, 0.0f, 1.0f}},
        {corners[5], {0.0f, 0.0f, 1.0f}},
        {corners[6], {0.0f, 0.0f, 1.0f}},
        {corners[7], {0.0f, 0.0f, 1.0f}},
    };

    mesh.indices = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        1, 5, 6, 1, 6, 2,
        2, 6, 7, 2, 7, 3,
        3, 7, 4, 3, 4, 0,
    };

    const float safe_chord = std::max(chord, 1.0e-4f);
    if (safe_chord < 0.02f) {
        const size_t base_count = mesh.vertices.size();
        for (size_t i = 0; i < base_count; ++i) {
            mesh.vertices.push_back(mesh.vertices[i]);
        }
    }

    return mesh;
}

}  // namespace geometry
