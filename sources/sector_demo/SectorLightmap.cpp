#include "sector_demo/SectorLightmap.h"

#include "sector_demo/SectorGeneratedGeometry.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace game {

namespace {

struct BakeTriangle {
    Vector3 worldPosition0 = {};
    Vector3 worldPosition1 = {};
    Vector3 worldPosition2 = {};
    Vector3 normal = {};
    Vector2 lightmapUv0 = {};
    Vector2 lightmapUv1 = {};
    Vector2 lightmapUv2 = {};
    int surfaceIndex = -1;
    int triangleIndex = -1;
};

struct RasterHit {
    bool hit = false;
    Vector3 position = {};
    Vector3 normal = {};
    int triangleIndex = -1;
};

struct RayHit {
    bool hit = false;
    float distance = 0.0f;
    Vector3 normal = {};
    Vector2 lightmapUv = {};
    int surfaceIndex = -1;
    int triangleIndex = -1;
    float barycentric0 = 0.0f;
    float barycentric1 = 0.0f;
    float barycentric2 = 0.0f;
};

struct BakeTexel {
    int x = 0;
    int y = 0;
    size_t pixelIndex = 0;
    int surfaceIndex = -1;
    int triangleIndex = -1;
    Vector3 position = {};
    Vector3 normal = {};
};

struct BakeAabb {
    Vector3 min = {};
    Vector3 max = {};
};

struct BakeBvhNode {
    BakeAabb bounds;
    int leftChild = -1;
    int rightChild = -1;
    int firstTriangle = 0;
    int triangleCount = 0;

    bool IsLeaf() const
    {
        return leftChild < 0 && rightChild < 0;
    }
};

struct SectorLightmapBvh {
    std::vector<BakeBvhNode> nodes;
    std::vector<int> orderedTriangleIndices;
};

struct BakeBvhBuildStats {
    int leafCount = 0;
    int maxTrianglesInLeaf = 0;
    int totalLeafTriangles = 0;
};

struct BakeRayStats {
    SectorLightmapRaycastStats directHardShadow;
    SectorLightmapRaycastStats softShadowSource;
    SectorLightmapRaycastStats ambientOcclusion;
    SectorLightmapRaycastStats indirectBounce;
};

constexpr float BakeEpsilon = 0.0001f;
constexpr float RayOriginEpsilon = 0.01f;
constexpr float RayHitEpsilon = 0.001f;
constexpr float BvhAabbEpsilon = 0.00001f;
constexpr int kSectorLightmapBvhLeafTriangleCount = 4;
constexpr int kSectorLightmapBvhTraversalStackSize = 128;
constexpr uint32_t kSectorLightmapProgressChunk = 512;
constexpr float Pi = 3.14159265358979323846f;

bool RayIntersectsTriangle(
        Vector3 origin,
        Vector3 direction,
        const BakeTriangle& tri,
        float maxDistance,
        float& outDistance,
        float& outBarycentric0,
        float& outBarycentric1,
        float& outBarycentric2);

uint64_t FnvAppendByte(uint64_t hash, uint8_t value)
{
    hash ^= value;
    hash *= 1099511628211ull;
    return hash;
}

void FnvAppendString(uint64_t& hash, const std::string& value)
{
    for (char ch : value) {
        hash = FnvAppendByte(hash, static_cast<uint8_t>(ch));
    }
    hash = FnvAppendByte(hash, 0xffu);
}

void FnvAppendInt(uint64_t& hash, int value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        hash = FnvAppendByte(hash, static_cast<uint8_t>((static_cast<uint32_t>(value) >> shift) & 0xffu));
    }
}

void FnvAppendFloat(uint64_t& hash, float value)
{
    FnvAppendInt(hash, static_cast<int>(std::lround(value * 10000.0f)));
}

std::string HashToString(uint64_t hash)
{
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(hash));
    return buffer;
}

unsigned char FloatToByte(float value)
{
    return static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(value * 255.0f)), 0, 255));
}

float Cross2(Vector2 a, Vector2 b, Vector2 c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool Barycentric(Vector2 point, Vector2 a, Vector2 b, Vector2 c, float& wa, float& wb, float& wc)
{
    const float denom = Cross2(a, b, c);
    if (std::fabs(denom) <= BakeEpsilon) {
        return false;
    }

    wa = Cross2(point, b, c) / denom;
    wb = Cross2(point, c, a) / denom;
    wc = 1.0f - wa - wb;
    const float tolerance = -0.001f;
    return wa >= tolerance && wb >= tolerance && wc >= tolerance;
}

Vector3 Interpolate(Vector3 a, Vector3 b, Vector3 c, float wa, float wb, float wc)
{
    return Vector3{
            a.x * wa + b.x * wb + c.x * wc,
            a.y * wa + b.y * wb + c.y * wc,
            a.z * wa + b.z * wb + c.z * wc
    };
}

bool RasterizeSurfacePoint(
        const SectorGeneratedSurface& surface,
        Vector2 localPoint,
        RasterHit& outHit)
{
    for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
        const SectorGeneratedVertex& va = surface.vertices[i + 0];
        const SectorGeneratedVertex& vb = surface.vertices[i + 1];
        const SectorGeneratedVertex& vc = surface.vertices[i + 2];
        float wa = 0.0f;
        float wb = 0.0f;
        float wc = 0.0f;
        if (!Barycentric(localPoint, va.chartUv, vb.chartUv, vc.chartUv, wa, wb, wc)) {
            continue;
        }

        outHit.hit = true;
        outHit.position = Interpolate(va.position, vb.position, vc.position, wa, wb, wc);
        outHit.normal = Vector3Normalize(Interpolate(va.normal, vb.normal, vc.normal, wa, wb, wc));
        outHit.triangleIndex = static_cast<int>(i / 3);
        return true;
    }

    return false;
}

bool RayIntersectsTriangle(Vector3 origin, Vector3 direction, const BakeTriangle& tri, float maxDistance, float& outDistance)
{
    float barycentric0 = 0.0f;
    float barycentric1 = 0.0f;
    float barycentric2 = 0.0f;
    return RayIntersectsTriangle(origin, direction, tri, maxDistance, outDistance, barycentric0, barycentric1, barycentric2);
}

bool RayIntersectsTriangle(
        Vector3 origin,
        Vector3 direction,
        const BakeTriangle& tri,
        float maxDistance,
        float& outDistance,
        float& outBarycentric0,
        float& outBarycentric1,
        float& outBarycentric2)
{
    const Vector3 edge1 = Vector3Subtract(tri.worldPosition1, tri.worldPosition0);
    const Vector3 edge2 = Vector3Subtract(tri.worldPosition2, tri.worldPosition0);
    const Vector3 h = Vector3CrossProduct(direction, edge2);
    const float det = Vector3DotProduct(edge1, h);
    if (std::fabs(det) <= 0.0000001f) {
        return false;
    }

    const float invDet = 1.0f / det;
    const Vector3 s = Vector3Subtract(origin, tri.worldPosition0);
    const float u = invDet * Vector3DotProduct(s, h);
    if (u < -0.0001f || u > 1.0001f) {
        return false;
    }

    const Vector3 q = Vector3CrossProduct(s, edge1);
    const float v = invDet * Vector3DotProduct(direction, q);
    if (v < -0.0001f || u + v > 1.0001f) {
        return false;
    }

    const float t = invDet * Vector3DotProduct(edge2, q);
    if (t > RayHitEpsilon && t < maxDistance) {
        outDistance = t;
        outBarycentric0 = 1.0f - u - v;
        outBarycentric1 = u;
        outBarycentric2 = v;
        return true;
    }
    return false;
}

bool RayIntersectsTriangle(Vector3 origin, Vector3 direction, const BakeTriangle& tri, float maxDistance)
{
    float distance = 0.0f;
    return RayIntersectsTriangle(origin, direction, tri, maxDistance, distance);
}

bool IsFinite(Vector3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

BakeAabb EmptyAabb()
{
    return BakeAabb{
            Vector3{
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()
            },
            Vector3{
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max()
            }
    };
}

void ExpandAabb(BakeAabb& bounds, Vector3 point)
{
    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.min.z = std::min(bounds.min.z, point.z);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
    bounds.max.z = std::max(bounds.max.z, point.z);
}

BakeAabb TriangleBounds(const BakeTriangle& tri)
{
    BakeAabb bounds = EmptyAabb();
    ExpandAabb(bounds, tri.worldPosition0);
    ExpandAabb(bounds, tri.worldPosition1);
    ExpandAabb(bounds, tri.worldPosition2);
    return bounds;
}

Vector3 TriangleCentroid(const BakeTriangle& tri)
{
    return Vector3Scale(
            Vector3Add(Vector3Add(tri.worldPosition0, tri.worldPosition1), tri.worldPosition2),
            1.0f / 3.0f
    );
}

float AxisValue(Vector3 value, int axis)
{
    switch (axis) {
        case 0: return value.x;
        case 1: return value.y;
        default: return value.z;
    }
}

int LongestAxis(Vector3 extent)
{
    if (extent.x >= extent.y && extent.x >= extent.z) {
        return 0;
    }
    if (extent.y >= extent.z) {
        return 1;
    }
    return 2;
}

int BuildBvhNode(
        SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        int first,
        int count,
        int depth,
        BakeBvhBuildStats& stats,
        std::string& outError)
{
    if (depth >= kSectorLightmapBvhTraversalStackSize) {
        outError = "Bake failed: lightmap BVH depth exceeded traversal stack capacity";
        return -1;
    }

    BakeAabb bounds = EmptyAabb();
    BakeAabb centroidBounds = EmptyAabb();
    for (int i = first; i < first + count; ++i) {
        const BakeTriangle& tri = triangles[static_cast<size_t>(bvh.orderedTriangleIndices[static_cast<size_t>(i)])];
        const BakeAabb triBounds = TriangleBounds(tri);
        ExpandAabb(bounds, triBounds.min);
        ExpandAabb(bounds, triBounds.max);
        ExpandAabb(centroidBounds, TriangleCentroid(tri));
    }

    const int nodeIndex = static_cast<int>(bvh.nodes.size());
    bvh.nodes.push_back(BakeBvhNode{});
    BakeBvhNode& node = bvh.nodes.back();
    node.bounds = bounds;
    node.firstTriangle = first;
    node.triangleCount = count;

    const Vector3 centroidExtent = Vector3Subtract(centroidBounds.max, centroidBounds.min);
    const int splitAxis = LongestAxis(centroidExtent);
    if (count <= kSectorLightmapBvhLeafTriangleCount || AxisValue(centroidExtent, splitAxis) <= BvhAabbEpsilon) {
        ++stats.leafCount;
        stats.maxTrianglesInLeaf = std::max(stats.maxTrianglesInLeaf, count);
        stats.totalLeafTriangles += count;
        return nodeIndex;
    }

    const int mid = first + count / 2;
    std::stable_sort(
            bvh.orderedTriangleIndices.begin() + first,
            bvh.orderedTriangleIndices.begin() + first + count,
            [&](int lhs, int rhs) {
                const float lhsCentroid = AxisValue(TriangleCentroid(triangles[static_cast<size_t>(lhs)]), splitAxis);
                const float rhsCentroid = AxisValue(TriangleCentroid(triangles[static_cast<size_t>(rhs)]), splitAxis);
                if (std::fabs(lhsCentroid - rhsCentroid) > BvhAabbEpsilon) {
                    return lhsCentroid < rhsCentroid;
                }
                return lhs < rhs;
            });

    const int leftChild = BuildBvhNode(bvh, triangles, first, mid - first, depth + 1, stats, outError);
    if (leftChild < 0) {
        return -1;
    }
    const int rightChild = BuildBvhNode(bvh, triangles, mid, first + count - mid, depth + 1, stats, outError);
    if (rightChild < 0) {
        return -1;
    }

    bvh.nodes[static_cast<size_t>(nodeIndex)].leftChild = leftChild;
    bvh.nodes[static_cast<size_t>(nodeIndex)].rightChild = rightChild;
    bvh.nodes[static_cast<size_t>(nodeIndex)].triangleCount = 0;
    return nodeIndex;
}

bool BuildSectorLightmapBvh(
        const std::vector<BakeTriangle>& triangles,
        SectorLightmapBvh& outBvh,
        BakeBvhBuildStats& outStats,
        std::string& outError)
{
    outBvh = SectorLightmapBvh{};
    outStats = BakeBvhBuildStats{};

    if (triangles.empty()) {
        return true;
    }

    if (triangles.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        outError = "Bake failed: too many triangles for lightmap BVH";
        return false;
    }

    for (const BakeTriangle& tri : triangles) {
        if (!IsFinite(tri.worldPosition0) || !IsFinite(tri.worldPosition1) || !IsFinite(tri.worldPosition2)) {
            outError = "Bake failed: invalid triangle data for lightmap BVH";
            return false;
        }
    }

    outBvh.orderedTriangleIndices.reserve(triangles.size());
    for (size_t i = 0; i < triangles.size(); ++i) {
        outBvh.orderedTriangleIndices.push_back(static_cast<int>(i));
    }
    outBvh.nodes.reserve(triangles.size() * 2);

    return BuildBvhNode(outBvh, triangles, 0, static_cast<int>(triangles.size()), 0, outStats, outError) >= 0;
}

bool IntersectRayAabb(const Ray& ray, const BakeAabb& bounds, float maxDistance, float& outEntryDistance)
{
    if (maxDistance <= 0.0f || !std::isfinite(maxDistance)
            || !IsFinite(ray.position) || !IsFinite(ray.direction)
            || !IsFinite(bounds.min) || !IsFinite(bounds.max)) {
        return false;
    }

    float tMin = 0.0f;
    float tMax = maxDistance;

    for (int axis = 0; axis < 3; ++axis) {
        const float origin = AxisValue(ray.position, axis);
        const float direction = AxisValue(ray.direction, axis);
        const float minValue = AxisValue(bounds.min, axis) - BvhAabbEpsilon;
        const float maxValue = AxisValue(bounds.max, axis) + BvhAabbEpsilon;

        if (std::fabs(direction) <= BvhAabbEpsilon) {
            if (origin < minValue || origin > maxValue) {
                return false;
            }
            continue;
        }

        const float invDirection = 1.0f / direction;
        float t0 = (minValue - origin) * invDirection;
        float t1 = (maxValue - origin) * invDirection;
        if (t0 > t1) {
            std::swap(t0, t1);
        }

        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
        if (tMin > tMax) {
            return false;
        }
    }

    outEntryDistance = std::max(0.0f, tMin);
    return outEntryDistance < maxDistance;
}

bool IsIgnoredTriangle(const BakeTriangle& tri, int sourceSurfaceIndex, int sourceTriangleIndex)
{
    return tri.surfaceIndex == sourceSurfaceIndex && tri.triangleIndex == sourceTriangleIndex;
}

bool RaycastBakeTrianglesAnyHit(
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const Ray& ray,
        float maxDistance,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        SectorLightmapRaycastStats* stats)
{
    if (stats != nullptr) {
        ++stats->raysCast;
    }
    if (bvh.nodes.empty()) {
        return false;
    }

    std::array<int, kSectorLightmapBvhTraversalStackSize> stack{};
    int stackSize = 0;
    stack[stackSize++] = 0;

    while (stackSize > 0) {
        const BakeBvhNode& node = bvh.nodes[static_cast<size_t>(stack[--stackSize])];
        if (node.IsLeaf()) {
            for (int i = 0; i < node.triangleCount; ++i) {
                const int triangleIndex = bvh.orderedTriangleIndices[static_cast<size_t>(node.firstTriangle + i)];
                const BakeTriangle& tri = triangles[static_cast<size_t>(triangleIndex)];
                if (IsIgnoredTriangle(tri, sourceSurfaceIndex, sourceTriangleIndex)) {
                    continue;
                }
                if (stats != nullptr) {
                    ++stats->triangleTests;
                }
                if (RayIntersectsTriangle(ray.position, ray.direction, tri, maxDistance)) {
                    if (stats != nullptr) {
                        ++stats->triangleHits;
                    }
                    return true;
                }
            }
            continue;
        }

        float leftEntry = 0.0f;
        float rightEntry = 0.0f;
        bool hitLeft = false;
        bool hitRight = false;
        if (node.leftChild >= 0) {
            if (stats != nullptr) {
                ++stats->aabbTests;
            }
            hitLeft = IntersectRayAabb(ray, bvh.nodes[static_cast<size_t>(node.leftChild)].bounds, maxDistance, leftEntry);
            if (hitLeft && stats != nullptr) {
                ++stats->aabbHits;
            }
        }
        if (node.rightChild >= 0) {
            if (stats != nullptr) {
                ++stats->aabbTests;
            }
            hitRight = IntersectRayAabb(ray, bvh.nodes[static_cast<size_t>(node.rightChild)].bounds, maxDistance, rightEntry);
            if (hitRight && stats != nullptr) {
                ++stats->aabbHits;
            }
        }

        if (hitLeft && hitRight) {
            const int nearChild = leftEntry <= rightEntry ? node.leftChild : node.rightChild;
            const int farChild = leftEntry <= rightEntry ? node.rightChild : node.leftChild;
            stack[stackSize++] = farChild;
            stack[stackSize++] = nearChild;
        } else if (hitLeft) {
            stack[stackSize++] = node.leftChild;
        } else if (hitRight) {
            stack[stackSize++] = node.rightChild;
        }
    }

    return false;
}

RayHit RaycastBakeTrianglesClosest(
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const Ray& ray,
        float maxDistance,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        SectorLightmapRaycastStats* stats)
{
    if (stats != nullptr) {
        ++stats->raysCast;
    }

    RayHit closest{};
    closest.distance = maxDistance;
    if (bvh.nodes.empty()) {
        return closest;
    }

    std::array<int, kSectorLightmapBvhTraversalStackSize> stack{};
    int stackSize = 0;
    stack[stackSize++] = 0;

    while (stackSize > 0) {
        const BakeBvhNode& node = bvh.nodes[static_cast<size_t>(stack[--stackSize])];
        if (node.IsLeaf()) {
            for (int i = 0; i < node.triangleCount; ++i) {
                const int triangleIndex = bvh.orderedTriangleIndices[static_cast<size_t>(node.firstTriangle + i)];
                const BakeTriangle& tri = triangles[static_cast<size_t>(triangleIndex)];
                if (IsIgnoredTriangle(tri, sourceSurfaceIndex, sourceTriangleIndex)) {
                    continue;
                }

                float distance = 0.0f;
                float barycentric0 = 0.0f;
                float barycentric1 = 0.0f;
                float barycentric2 = 0.0f;
                if (stats != nullptr) {
                    ++stats->triangleTests;
                }
                if (RayIntersectsTriangle(ray.position, ray.direction, tri, closest.distance, distance, barycentric0, barycentric1, barycentric2)) {
                    if (stats != nullptr) {
                        ++stats->triangleHits;
                    }
                    closest.hit = true;
                    closest.distance = distance;
                    closest.normal = tri.normal;
                    closest.surfaceIndex = tri.surfaceIndex;
                    closest.triangleIndex = tri.triangleIndex;
                    closest.barycentric0 = barycentric0;
                    closest.barycentric1 = barycentric1;
                    closest.barycentric2 = barycentric2;
                    closest.lightmapUv = Vector2{
                            tri.lightmapUv0.x * barycentric0 + tri.lightmapUv1.x * barycentric1 + tri.lightmapUv2.x * barycentric2,
                            tri.lightmapUv0.y * barycentric0 + tri.lightmapUv1.y * barycentric1 + tri.lightmapUv2.y * barycentric2
                    };
                }
            }
            continue;
        }

        float leftEntry = 0.0f;
        float rightEntry = 0.0f;
        bool hitLeft = false;
        bool hitRight = false;
        if (node.leftChild >= 0) {
            if (stats != nullptr) {
                ++stats->aabbTests;
            }
            hitLeft = IntersectRayAabb(ray, bvh.nodes[static_cast<size_t>(node.leftChild)].bounds, closest.distance, leftEntry);
            if (hitLeft && stats != nullptr) {
                ++stats->aabbHits;
            }
        }
        if (node.rightChild >= 0) {
            if (stats != nullptr) {
                ++stats->aabbTests;
            }
            hitRight = IntersectRayAabb(ray, bvh.nodes[static_cast<size_t>(node.rightChild)].bounds, closest.distance, rightEntry);
            if (hitRight && stats != nullptr) {
                ++stats->aabbHits;
            }
        }

        if (hitLeft && leftEntry >= closest.distance) {
            hitLeft = false;
        }
        if (hitRight && rightEntry >= closest.distance) {
            hitRight = false;
        }

        if (hitLeft && hitRight) {
            const int nearChild = leftEntry <= rightEntry ? node.leftChild : node.rightChild;
            const int farChild = leftEntry <= rightEntry ? node.rightChild : node.leftChild;
            stack[stackSize++] = farChild;
            stack[stackSize++] = nearChild;
        } else if (hitLeft) {
            stack[stackSize++] = node.leftChild;
        } else if (hitRight) {
            stack[stackSize++] = node.rightChild;
        }
    }

    return closest;
}

bool IsOccluded(
        Vector3 position,
        Vector3 normal,
        Vector3 lightPosition,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        bool softShadowSample,
        BakeRayStats& stats)
{
    const Vector3 toLight = Vector3Subtract(lightPosition, position);
    const float distance = Vector3Length(toLight);
    if (distance <= RayHitEpsilon) {
        return false;
    }

    const Vector3 direction = Vector3Scale(toLight, 1.0f / distance);
    const Vector3 origin = Vector3Add(position, Vector3Scale(normal, RayOriginEpsilon));
    const float maxDistance = std::max(0.0f, distance - RayOriginEpsilon * 2.0f);
    const Ray ray{origin, direction};

    return RaycastBakeTrianglesAnyHit(
            bvh,
            triangles,
            ray,
            maxDistance,
            sourceSurfaceIndex,
            sourceTriangleIndex,
            softShadowSample ? &stats.softShadowSource : &stats.directHardShadow
    );
}

RayHit TraceRay(
        Vector3 origin,
        Vector3 direction,
        float maxDistance,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        SectorLightmapRaycastStats* stats,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles)
{
    return RaycastBakeTrianglesClosest(
            bvh,
            triangles,
            Ray{origin, direction},
            maxDistance,
            sourceSurfaceIndex,
            sourceTriangleIndex,
            stats
    );
}

float RadicalInverseBase2(unsigned int value)
{
    value = (value << 16u) | (value >> 16u);
    value = ((value & 0x55555555u) << 1u) | ((value & 0xaaaaaaaau) >> 1u);
    value = ((value & 0x33333333u) << 2u) | ((value & 0xccccccccu) >> 2u);
    value = ((value & 0x0f0f0f0fu) << 4u) | ((value & 0xf0f0f0f0u) >> 4u);
    value = ((value & 0x00ff00ffu) << 8u) | ((value & 0xff00ff00u) >> 8u);
    return static_cast<float>(value) * 2.3283064365386963e-10f;
}

void BuildOrthonormalBasis(Vector3 normal, Vector3& tangent, Vector3& bitangent)
{
    const Vector3 up = std::fabs(normal.y) < 0.999f ? Vector3{0.0f, 1.0f, 0.0f} : Vector3{1.0f, 0.0f, 0.0f};
    tangent = Vector3Normalize(Vector3CrossProduct(up, normal));
    bitangent = Vector3CrossProduct(normal, tangent);
}

Vector3 FibonacciSphereSample(int sampleIndex, int sampleCount)
{
    const float goldenAngle = Pi * (3.0f - std::sqrt(5.0f));
    const float y = 1.0f - (2.0f * (static_cast<float>(sampleIndex) + 0.5f) / static_cast<float>(sampleCount));
    const float radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
    const float theta = goldenAngle * static_cast<float>(sampleIndex);
    return Vector3{std::cos(theta) * radius, y, std::sin(theta) * radius};
}

Vector3 CosineHemisphereSample(Vector3 normal, int sampleIndex, int sampleCount)
{
    Vector3 tangent{};
    Vector3 bitangent{};
    BuildOrthonormalBasis(normal, tangent, bitangent);

    const float u = (static_cast<float>(sampleIndex) + 0.5f) / static_cast<float>(sampleCount);
    const float v = RadicalInverseBase2(static_cast<unsigned int>(sampleIndex + 1));
    const float r = std::sqrt(u);
    const float theta = 2.0f * Pi * v;
    const float x = r * std::cos(theta);
    const float z = r * std::sin(theta);
    const float y = std::sqrt(std::max(0.0f, 1.0f - u));

    return Vector3Normalize(Vector3Add(
            Vector3Add(Vector3Scale(tangent, x), Vector3Scale(normal, y)),
            Vector3Scale(bitangent, z)
    ));
}

Vector3 EvaluateDirectLightSample(
        const SectorStaticPointLight& light,
        Vector3 lightPosition,
        const RasterHit& hit,
        int surfaceIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        bool softShadowSample,
        BakeRayStats& stats)
{
    const Vector3 toLight = Vector3Subtract(lightPosition, hit.position);
    const float distance = Vector3Length(toLight);
    if (distance <= RayHitEpsilon || distance > light.radius) {
        return Vector3{};
    }

    const Vector3 lightDir = Vector3Scale(toLight, 1.0f / distance);
    const float lambert = std::max(Vector3DotProduct(hit.normal, lightDir), 0.0f);
    if (lambert <= 0.0f) {
        return Vector3{};
    }

    if (IsOccluded(hit.position, hit.normal, lightPosition, surfaceIndex, hit.triangleIndex, bvh, triangles, softShadowSample, stats)) {
        return Vector3{};
    }

    const float t = std::clamp(1.0f - distance / light.radius, 0.0f, 1.0f);
    const float attenuation = t * t;
    const float scale = light.intensity * attenuation * lambert;
    return Vector3{
            (static_cast<float>(light.color.r) / 255.0f) * scale,
            (static_cast<float>(light.color.g) / 255.0f) * scale,
            (static_cast<float>(light.color.b) / 255.0f) * scale
    };
}

Vector3 EvaluateDirectLight(
        const SectorStaticPointLight& light,
        const RasterHit& hit,
        int surfaceIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        BakeRayStats& stats)
{
    if (light.radius <= 0.0f || light.intensity <= 0.0f) {
        return Vector3{};
    }

    const float sourceRadius = std::min(std::clamp(light.sourceRadius, 0.0f, 8.0f), light.radius * 0.5f);
    if (sourceRadius <= BakeEpsilon) {
        return EvaluateDirectLightSample(light, light.position, hit, surfaceIndex, bvh, triangles, false, stats);
    }

    Vector3 direct{};
    for (int i = 0; i < kDirectSoftShadowSampleCount; ++i) {
        const Vector3 sampleOffset = Vector3Scale(FibonacciSphereSample(i, kDirectSoftShadowSampleCount), sourceRadius);
        const Vector3 samplePosition = Vector3Add(light.position, sampleOffset);
        direct = Vector3Add(direct, EvaluateDirectLightSample(light, samplePosition, hit, surfaceIndex, bvh, triangles, true, stats));
    }
    return Vector3Scale(direct, 1.0f / static_cast<float>(kDirectSoftShadowSampleCount));
}

float BakeAmbientOcclusion(
        const RasterHit& hit,
        int surfaceIndex,
        float radius,
        float strength,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        BakeRayStats& stats)
{
    if (strength <= 0.0f || radius <= BakeEpsilon) {
        return 1.0f;
    }

    const Vector3 origin = Vector3Add(hit.position, Vector3Scale(hit.normal, RayOriginEpsilon));
    float occlusion = 0.0f;
    for (int i = 0; i < kAmbientOcclusionSampleCount; ++i) {
        const Vector3 direction = CosineHemisphereSample(hit.normal, i, kAmbientOcclusionSampleCount);
        const RayHit rayHit = TraceRay(origin, direction, radius, surfaceIndex, hit.triangleIndex, &stats.ambientOcclusion, bvh, triangles);
        if (!rayHit.hit) {
            continue;
        }

        occlusion += 1.0f - std::clamp(rayHit.distance / radius, 0.0f, 1.0f);
    }

    const float averageOcclusion = occlusion / static_cast<float>(kAmbientOcclusionSampleCount);
    return std::clamp(1.0f - strength * averageOcclusion, 0.0f, 1.0f);
}

std::vector<BakeTriangle> BuildBakeTriangles(const SectorGeneratedGeometry& geometry, const SectorLightmapLayout& layout)
{
    std::vector<BakeTriangle> triangles;
    for (size_t surfaceIndex = 0; surfaceIndex < geometry.surfaces.size(); ++surfaceIndex) {
        if (surfaceIndex >= layout.charts.size()) {
            continue;
        }
        const SectorGeneratedSurface& surface = geometry.surfaces[surfaceIndex];
        const SectorLightmapChart& chart = layout.charts[surfaceIndex];
        for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
            Vector3 normal = surface.normal;
            if (Vector3LengthSqr(normal) <= BakeEpsilon) {
                const Vector3 edge1 = Vector3Subtract(surface.vertices[i + 1].position, surface.vertices[i + 0].position);
                const Vector3 edge2 = Vector3Subtract(surface.vertices[i + 2].position, surface.vertices[i + 0].position);
                normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
            }
            const Vector2 uv0 = i + 0 < chart.vertexUvs.size() ? chart.vertexUvs[i + 0] : Vector2{};
            const Vector2 uv1 = i + 1 < chart.vertexUvs.size() ? chart.vertexUvs[i + 1] : Vector2{};
            const Vector2 uv2 = i + 2 < chart.vertexUvs.size() ? chart.vertexUvs[i + 2] : Vector2{};
            triangles.push_back(BakeTriangle{
                    surface.vertices[i + 0].position,
                    surface.vertices[i + 1].position,
                    surface.vertices[i + 2].position,
                    Vector3Normalize(normal),
                    uv0,
                    uv1,
                    uv2,
                    static_cast<int>(surfaceIndex),
                    static_cast<int>(i / 3)
            });
        }
    }
    return triangles;
}

Vector2 ChartLocalToAtlasUv(const SectorLightmapChart& chart, const SectorGeneratedSurface& surface, Vector2 local)
{
    const float localU = surface.chartWidth <= BakeEpsilon ? 0.0f : std::clamp(local.x / surface.chartWidth, 0.0f, 1.0f);
    const float localV = surface.chartHeight <= BakeEpsilon ? 0.0f : std::clamp(local.y / surface.chartHeight, 0.0f, 1.0f);
    const float minX = static_cast<float>(chart.usableX) + 0.5f;
    const float minY = static_cast<float>(chart.usableY) + 0.5f;
    const float maxX = static_cast<float>(chart.usableX + chart.usableWidth) - 0.5f;
    const float maxY = static_cast<float>(chart.usableY + chart.usableHeight) - 0.5f;
    return Vector2{
            (minX + (maxX - minX) * localU) / static_cast<float>(SectorLightmapAtlasWidth),
            (minY + (maxY - minY) * localV) / static_cast<float>(SectorLightmapAtlasHeight)
    };
}

void SetPixel(std::vector<Color>& pixels, int width, int x, int y, Color color)
{
    if (x < 0 || y < 0 || x >= width) {
        return;
    }
    pixels[static_cast<size_t>(y * width + x)] = color;
}

Color GetPixel(const std::vector<Color>& pixels, int width, int x, int y)
{
    return pixels[static_cast<size_t>(y * width + x)];
}

void DilateChart(
        const SectorLightmapChart& chart,
        std::vector<Color>& pixels,
        std::vector<unsigned char>& valid,
        int atlasWidth)
{
    for (int y = chart.y; y < chart.y + chart.height; ++y) {
        for (int x = chart.x; x < chart.x + chart.width; ++x) {
            const size_t index = static_cast<size_t>(y * atlasWidth + x);
            if (valid[index] != 0) {
                continue;
            }

            int bestDistance2 = std::numeric_limits<int>::max();
            Color best = BLACK;
            for (int sy = chart.usableY; sy < chart.usableY + chart.usableHeight; ++sy) {
                for (int sx = chart.usableX; sx < chart.usableX + chart.usableWidth; ++sx) {
                    const size_t sourceIndex = static_cast<size_t>(sy * atlasWidth + sx);
                    if (valid[sourceIndex] == 0) {
                        continue;
                    }
                    const int dx = sx - x;
                    const int dy = sy - y;
                    const int distance2 = dx * dx + dy * dy;
                    if (distance2 < bestDistance2) {
                        bestDistance2 = distance2;
                        best = pixels[sourceIndex];
                    }
                }
            }

            pixels[index] = best;
        }
    }
}

void DilateChartFloat(
        const SectorLightmapChart& chart,
        std::vector<Vector3>& values,
        std::vector<unsigned char>& valid,
        int atlasWidth)
{
    for (int y = chart.y; y < chart.y + chart.height; ++y) {
        for (int x = chart.x; x < chart.x + chart.width; ++x) {
            const size_t index = static_cast<size_t>(y * atlasWidth + x);
            if (valid[index] != 0) {
                continue;
            }

            int bestDistance2 = std::numeric_limits<int>::max();
            Vector3 best{};
            for (int sy = chart.usableY; sy < chart.usableY + chart.usableHeight; ++sy) {
                for (int sx = chart.usableX; sx < chart.usableX + chart.usableWidth; ++sx) {
                    const size_t sourceIndex = static_cast<size_t>(sy * atlasWidth + sx);
                    if (valid[sourceIndex] == 0) {
                        continue;
                    }
                    const int dx = sx - x;
                    const int dy = sy - y;
                    const int distance2 = dx * dx + dy * dy;
                    if (distance2 < bestDistance2) {
                        bestDistance2 = distance2;
                        best = values[sourceIndex];
                    }
                }
            }

            if (bestDistance2 != std::numeric_limits<int>::max()) {
                values[index] = best;
                valid[index] = 1;
            }
        }
    }
}

Vector3 SampleDirectLightingAtLightmapUv(
        const std::vector<Vector3>& directLightingFloat,
        const std::vector<unsigned char>& valid,
        int atlasWidth,
        int atlasHeight,
        Vector2 lightmapUv)
{
    const float pixelX = std::clamp(lightmapUv.x * static_cast<float>(atlasWidth) - 0.5f, 0.0f, static_cast<float>(atlasWidth - 1));
    const float pixelY = std::clamp(lightmapUv.y * static_cast<float>(atlasHeight) - 0.5f, 0.0f, static_cast<float>(atlasHeight - 1));
    const int x0 = static_cast<int>(std::floor(pixelX));
    const int y0 = static_cast<int>(std::floor(pixelY));
    const int x1 = std::min(x0 + 1, atlasWidth - 1);
    const int y1 = std::min(y0 + 1, atlasHeight - 1);
    const float tx = pixelX - static_cast<float>(x0);
    const float ty = pixelY - static_cast<float>(y0);

    const struct Sample {
        int x;
        int y;
        float weight;
    } samples[] = {
            {x0, y0, (1.0f - tx) * (1.0f - ty)},
            {x1, y0, tx * (1.0f - ty)},
            {x0, y1, (1.0f - tx) * ty},
            {x1, y1, tx * ty}
    };

    Vector3 sum{};
    float weightSum = 0.0f;
    for (const Sample& sample : samples) {
        const size_t index = static_cast<size_t>(sample.y * atlasWidth + sample.x);
        if (valid[index] == 0 || sample.weight <= 0.0f) {
            continue;
        }
        sum = Vector3Add(sum, Vector3Scale(directLightingFloat[index], sample.weight));
        weightSum += sample.weight;
    }
    if (weightSum > BakeEpsilon) {
        return Vector3Scale(sum, 1.0f / weightSum);
    }

    const int nearestX = static_cast<int>(std::lround(pixelX));
    const int nearestY = static_cast<int>(std::lround(pixelY));
    for (int radius = 0; radius <= SectorLightmapGutterTexels + 2; ++radius) {
        for (int y = std::max(0, nearestY - radius); y <= std::min(atlasHeight - 1, nearestY + radius); ++y) {
            for (int x = std::max(0, nearestX - radius); x <= std::min(atlasWidth - 1, nearestX + radius); ++x) {
                const size_t index = static_cast<size_t>(y * atlasWidth + x);
                if (valid[index] != 0) {
                    return directLightingFloat[index];
                }
            }
        }
    }

    return Vector3{};
}

bool FileExistsResolved(const std::string& path)
{
    std::ifstream file(path);
    return static_cast<bool>(file);
}

void ReportProgress(
        const SectorLightmapBakeCallbacks& callbacks,
        SectorLightmapBakePhase phase,
        uint32_t completedWork,
        uint32_t totalWork)
{
    if (callbacks.onProgress) {
        callbacks.onProgress(phase, completedWork, totalWork);
    }
}

bool IsBakeCancellationRequested(const SectorLightmapBakeCallbacks& callbacks)
{
    return callbacks.isCancellationRequested && callbacks.isCancellationRequested();
}

bool CheckBakeCancelled(const SectorLightmapBakeCallbacks& callbacks, std::string& outError)
{
    if (!IsBakeCancellationRequested(callbacks)) {
        return false;
    }
    outError = "Bake cancelled";
    return true;
}

} // namespace

std::string ResolveSectorAssetPath(const std::string& path)
{
    const std::string prefix = "assets/";
    if (path.compare(0, prefix.size(), prefix) == 0) {
        return std::string(ASSETS_PATH) + path.substr(prefix.size());
    }
    return path;
}

std::string MakeSectorAssetRelativePath(const std::string& path)
{
    const std::filesystem::path assetsRoot = std::filesystem::path(ASSETS_PATH).lexically_normal();
    const std::filesystem::path absolute = std::filesystem::path(path).lexically_normal();
    std::error_code ec;
    const std::filesystem::path relative = std::filesystem::relative(absolute, assetsRoot, ec);
    if (!ec && !relative.empty() && relative.native().find("..") != 0) {
        return std::string{"assets/"} + relative.generic_string();
    }
    return std::filesystem::path(path).generic_string();
}

std::string MakeSectorLightmapPathForMapPath(const std::string& mapPath)
{
    std::filesystem::path path(mapPath);
    path.replace_extension(".lightmap.png");
    return MakeSectorAssetRelativePath(path.generic_string());
}

bool BuildSectorLightmapLayout(
        const SectorMap& map,
        SectorLightmapLayout& outLayout,
        std::string& outError)
{
    outError.clear();
    outLayout = SectorLightmapLayout{};

    SectorGeneratedGeometry geometry;
    if (!BuildSectorGeneratedGeometry(map, geometry)) {
        outError = "Bake failed: no generated sector surfaces";
        return false;
    }

    outLayout.charts.resize(geometry.surfaces.size());
    int shelfX = 0;
    int shelfY = 0;
    int shelfHeight = 0;

    for (size_t surfaceIndex = 0; surfaceIndex < geometry.surfaces.size(); ++surfaceIndex) {
        const SectorGeneratedSurface& surface = geometry.surfaces[surfaceIndex];
        const int usableWidth = std::max(2, static_cast<int>(std::ceil(surface.chartWidth * SectorLightmapTexelsPerWorldUnit)));
        const int usableHeight = std::max(2, static_cast<int>(std::ceil(surface.chartHeight * SectorLightmapTexelsPerWorldUnit)));
        const int chartWidth = usableWidth + SectorLightmapGutterTexels * 2;
        const int chartHeight = usableHeight + SectorLightmapGutterTexels * 2;

        if (chartWidth > SectorLightmapAtlasWidth || chartHeight > SectorLightmapAtlasHeight) {
            outError = "Bake failed: a lightmap chart is larger than the 2048 atlas";
            return false;
        }

        if (shelfX + chartWidth > SectorLightmapAtlasWidth) {
            shelfX = 0;
            shelfY += shelfHeight;
            shelfHeight = 0;
        }

        if (shelfY + chartHeight > SectorLightmapAtlasHeight) {
            outError = "Bake failed: 2048 lightmap atlas is full";
            return false;
        }

        SectorLightmapChart chart;
        chart.surfaceIndex = static_cast<int>(surfaceIndex);
        chart.x = shelfX;
        chart.y = shelfY;
        chart.width = chartWidth;
        chart.height = chartHeight;
        chart.usableX = shelfX + SectorLightmapGutterTexels;
        chart.usableY = shelfY + SectorLightmapGutterTexels;
        chart.usableWidth = usableWidth;
        chart.usableHeight = usableHeight;
        chart.vertexUvs.reserve(surface.vertices.size());
        for (const SectorGeneratedVertex& vertex : surface.vertices) {
            chart.vertexUvs.push_back(ChartLocalToAtlasUv(chart, surface, vertex.chartUv));
        }
        outLayout.charts[surfaceIndex] = std::move(chart);

        shelfX += chartWidth;
        shelfHeight = std::max(shelfHeight, chartHeight);
    }

    return true;
}

bool BakeSectorLightmap(
        const SectorMap& map,
        const SectorLightmapLayout& layout,
        const char* outputPath,
        SectorLightmapBakeResult& outResult,
        std::string& outError)
{
    SectorLightmapBakeCallbacks callbacks;
    return BakeSectorLightmap(map, layout, outputPath, callbacks, outResult, outError);
}

bool BakeSectorLightmap(
        const SectorMap& map,
        const SectorLightmapLayout& layout,
        const char* outputPath,
        const SectorLightmapBakeCallbacks& callbacks,
        SectorLightmapBakeResult& outResult,
        std::string& outError)
{
    outResult = SectorLightmapBakeResult{};
    outError.clear();
    if (outputPath == nullptr || outputPath[0] == '\0') {
        outError = "Bake failed: missing output path";
        return false;
    }

    SectorGeneratedGeometry geometry;
    if (!BuildSectorGeneratedGeometry(map, geometry)) {
        outError = "Bake failed: no generated sector surfaces";
        return false;
    }
    if (layout.charts.size() != geometry.surfaces.size()) {
        outError = "Bake failed: lightmap layout does not match generated geometry";
        return false;
    }

    using Clock = std::chrono::steady_clock;
    const auto totalStart = Clock::now();
    const int width = layout.atlasWidth;
    const int height = layout.atlasHeight;
    const size_t atlasPixelCount = static_cast<size_t>(width * height);
    ReportProgress(callbacks, SectorLightmapBakePhase::Preparing, 0, 1);
    std::vector<Color> pixels(static_cast<size_t>(width * height), Color{0, 0, 0, 255});
    std::vector<Vector3> directLightingFloat(atlasPixelCount, Vector3{});
    std::vector<Vector3> indirectLightingFloat(atlasPixelCount, Vector3{});
    std::vector<float> ambientOcclusionFloat(atlasPixelCount, 1.0f);
    std::vector<unsigned char> validChartTexel(atlasPixelCount, 0);
    std::vector<BakeTexel> bakeTexels;
    const std::vector<BakeTriangle> triangles = BuildBakeTriangles(geometry, layout);
    ReportProgress(callbacks, SectorLightmapBakePhase::BuildingBvh, 0, 1);
    const auto bvhBuildStart = Clock::now();
    SectorLightmapBvh bvh;
    BakeBvhBuildStats bvhStats;
    if (!BuildSectorLightmapBvh(triangles, bvh, bvhStats, outError)) {
        if (outError.empty()) {
            outError = "Bake failed: could not build lightmap BVH";
        }
        return false;
    }
    const auto bvhBuildEnd = Clock::now();
    ReportProgress(callbacks, SectorLightmapBakePhase::BuildingBvh, 1, 1);
    if (CheckBakeCancelled(callbacks, outError)) {
        return false;
    }
    const float aoRadius = std::clamp(map.lightmapSettings.ambientOcclusionRadius, 0.05f, 16.0f);
    const float aoStrength = std::clamp(map.lightmapSettings.ambientOcclusionStrength, 0.0f, 1.0f);
    const float indirectBounceRadius = std::clamp(map.lightmapSettings.indirectBounceRadius, 0.05f, 16.0f);
    const float indirectBounceStrength = std::clamp(map.lightmapSettings.indirectBounceStrength, 0.0f, 1.0f);
    BakeRayStats stats;
    int allocatedChartRectanglePixels = 0;

    for (const SectorLightmapChart& chart : layout.charts) {
        allocatedChartRectanglePixels += chart.width * chart.height;
        if (chart.surfaceIndex < 0 || chart.surfaceIndex >= static_cast<int>(geometry.surfaces.size())) {
            continue;
        }

        const SectorGeneratedSurface& surface = geometry.surfaces[static_cast<size_t>(chart.surfaceIndex)];
        for (int y = chart.usableY; y < chart.usableY + chart.usableHeight; ++y) {
            for (int x = chart.usableX; x < chart.usableX + chart.usableWidth; ++x) {
                const float u = (static_cast<float>(x - chart.usableX) + 0.5f) / static_cast<float>(chart.usableWidth);
                const float v = (static_cast<float>(y - chart.usableY) + 0.5f) / static_cast<float>(chart.usableHeight);
                const Vector2 localPoint{u * surface.chartWidth, v * surface.chartHeight};

                RasterHit hit;
                if (!RasterizeSurfacePoint(surface, localPoint, hit)) {
                    continue;
                }

                const size_t pixelIndex = static_cast<size_t>(y * width + x);
                bakeTexels.push_back(BakeTexel{
                        x,
                        y,
                        pixelIndex,
                        chart.surfaceIndex,
                        hit.triangleIndex,
                        hit.position,
                        hit.normal
                });
                validChartTexel[pixelIndex] = 1;
            }
        }
    }
    ReportProgress(callbacks, SectorLightmapBakePhase::Preparing, 1, 1);
    if (CheckBakeCancelled(callbacks, outError)) {
        return false;
    }

    const auto directStart = Clock::now();
    ReportProgress(callbacks, SectorLightmapBakePhase::DirectLighting, 0, static_cast<uint32_t>(bakeTexels.size()));
    uint32_t completedTexels = 0;
    if (!map.staticLights.empty()) {
        for (const BakeTexel& texel : bakeTexels) {
            RasterHit hit;
            hit.hit = true;
            hit.position = texel.position;
            hit.normal = texel.normal;
            hit.triangleIndex = texel.triangleIndex;

            Vector3 direct{};
            for (const SectorStaticPointLight& light : map.staticLights) {
                direct = Vector3Add(direct, EvaluateDirectLight(light, hit, texel.surfaceIndex, bvh, triangles, stats));
            }
            directLightingFloat[texel.pixelIndex] = direct;
            ++completedTexels;
            if ((completedTexels % kSectorLightmapProgressChunk) == 0) {
                ReportProgress(callbacks, SectorLightmapBakePhase::DirectLighting, completedTexels, static_cast<uint32_t>(bakeTexels.size()));
                if (CheckBakeCancelled(callbacks, outError)) {
                    return false;
                }
            }
        }
    }
    ReportProgress(callbacks, SectorLightmapBakePhase::DirectLighting, static_cast<uint32_t>(bakeTexels.size()), static_cast<uint32_t>(bakeTexels.size()));
    const auto directEnd = Clock::now();

    const auto aoStart = Clock::now();
    ReportProgress(callbacks, SectorLightmapBakePhase::AmbientOcclusion, 0, static_cast<uint32_t>(bakeTexels.size()));
    completedTexels = 0;
    if (aoStrength > 0.0f) {
        for (const BakeTexel& texel : bakeTexels) {
            RasterHit hit;
            hit.hit = true;
            hit.position = texel.position;
            hit.normal = texel.normal;
            hit.triangleIndex = texel.triangleIndex;
            ambientOcclusionFloat[texel.pixelIndex] = BakeAmbientOcclusion(hit, texel.surfaceIndex, aoRadius, aoStrength, bvh, triangles, stats);
            ++completedTexels;
            if ((completedTexels % kSectorLightmapProgressChunk) == 0) {
                ReportProgress(callbacks, SectorLightmapBakePhase::AmbientOcclusion, completedTexels, static_cast<uint32_t>(bakeTexels.size()));
                if (CheckBakeCancelled(callbacks, outError)) {
                    return false;
                }
            }
        }
    }
    ReportProgress(callbacks, SectorLightmapBakePhase::AmbientOcclusion, static_cast<uint32_t>(bakeTexels.size()), static_cast<uint32_t>(bakeTexels.size()));
    const auto aoEnd = Clock::now();

    const auto indirectStart = Clock::now();
    ReportProgress(callbacks, SectorLightmapBakePhase::IndirectBounce, 0, static_cast<uint32_t>(bakeTexels.size()));
    if (indirectBounceStrength > 0.0f) {
        std::vector<Vector3> directSampleFloat = directLightingFloat;
        std::vector<unsigned char> directSampleValid = validChartTexel;
        for (const SectorLightmapChart& chart : layout.charts) {
            DilateChartFloat(chart, directSampleFloat, directSampleValid, width);
        }

        completedTexels = 0;
        for (const BakeTexel& texel : bakeTexels) {
            const Vector3 origin = Vector3Add(texel.position, Vector3Scale(texel.normal, RayOriginEpsilon));
            Vector3 gathered{};
            for (int i = 0; i < kIndirectBounceSampleCount; ++i) {
                const Vector3 direction = CosineHemisphereSample(texel.normal, i, kIndirectBounceSampleCount);
                const RayHit rayHit = TraceRay(
                        origin,
                        direction,
                        indirectBounceRadius,
                        texel.surfaceIndex,
                        texel.triangleIndex,
                        &stats.indirectBounce,
                        bvh,
                        triangles
                );
                if (!rayHit.hit) {
                    continue;
                }

                const Vector3 sampledDirect = SampleDirectLightingAtLightmapUv(
                        directSampleFloat,
                        directSampleValid,
                        width,
                        height,
                        rayHit.lightmapUv
                );
                const float distanceT = std::clamp(1.0f - rayHit.distance / indirectBounceRadius, 0.0f, 1.0f);
                const float distanceWeight = distanceT * distanceT;
                const float hitFacing = std::max(Vector3DotProduct(rayHit.normal, Vector3Negate(direction)), 0.0f);
                const float scale = kNeutralBounceAlbedo * distanceWeight * hitFacing;
                gathered = Vector3Add(gathered, Vector3Scale(sampledDirect, scale));
            }

            const float averageScale = indirectBounceStrength / static_cast<float>(kIndirectBounceSampleCount);
            indirectLightingFloat[texel.pixelIndex] = Vector3Scale(gathered, averageScale);
            ++completedTexels;
            if ((completedTexels % kSectorLightmapProgressChunk) == 0) {
                ReportProgress(callbacks, SectorLightmapBakePhase::IndirectBounce, completedTexels, static_cast<uint32_t>(bakeTexels.size()));
                if (CheckBakeCancelled(callbacks, outError)) {
                    return false;
                }
            }
        }
    }
    ReportProgress(callbacks, SectorLightmapBakePhase::IndirectBounce, static_cast<uint32_t>(bakeTexels.size()), static_cast<uint32_t>(bakeTexels.size()));
    const auto indirectEnd = Clock::now();

    const auto exportStart = Clock::now();
    ReportProgress(callbacks, SectorLightmapBakePhase::DilatingAndEncoding, 0, static_cast<uint32_t>(layout.charts.size() + bakeTexels.size()));
    std::vector<unsigned char> exportValid = validChartTexel;
    completedTexels = 0;
    for (const BakeTexel& texel : bakeTexels) {
        Vector3 finalRgb = Vector3Add(directLightingFloat[texel.pixelIndex], indirectLightingFloat[texel.pixelIndex]);
        finalRgb.x = std::clamp(finalRgb.x, 0.0f, 1.0f);
        finalRgb.y = std::clamp(finalRgb.y, 0.0f, 1.0f);
        finalRgb.z = std::clamp(finalRgb.z, 0.0f, 1.0f);
        pixels[texel.pixelIndex] = Color{
                FloatToByte(finalRgb.x),
                FloatToByte(finalRgb.y),
                FloatToByte(finalRgb.z),
                FloatToByte(ambientOcclusionFloat[texel.pixelIndex])
        };
        ++completedTexels;
        if ((completedTexels % kSectorLightmapProgressChunk) == 0) {
            ReportProgress(callbacks, SectorLightmapBakePhase::DilatingAndEncoding, completedTexels, static_cast<uint32_t>(layout.charts.size() + bakeTexels.size()));
            if (CheckBakeCancelled(callbacks, outError)) {
                return false;
            }
        }
    }
    uint32_t completedExportWork = static_cast<uint32_t>(bakeTexels.size());
    for (const SectorLightmapChart& chart : layout.charts) {
        DilateChart(chart, pixels, exportValid, width);
        ++completedExportWork;
        ReportProgress(callbacks, SectorLightmapBakePhase::DilatingAndEncoding, completedExportWork, static_cast<uint32_t>(layout.charts.size() + bakeTexels.size()));
        if (CheckBakeCancelled(callbacks, outError)) {
            return false;
        }
    }

    Image image = {};
    image.data = pixels.data();
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    const std::filesystem::path output(outputPath);
    std::error_code ec;
    if (!output.parent_path().empty()) {
        std::filesystem::create_directories(output.parent_path(), ec);
    }
    if (ec) {
        outError = TextFormat("Bake failed: could not create output directory: %s", ec.message().c_str());
        return false;
    }

    if (!ExportImage(image, outputPath)) {
        outError = TextFormat("Bake failed: could not export %s", outputPath);
        return false;
    }
    ReportProgress(callbacks, SectorLightmapBakePhase::DilatingAndEncoding, static_cast<uint32_t>(layout.charts.size() + bakeTexels.size()), static_cast<uint32_t>(layout.charts.size() + bakeTexels.size()));
    const auto exportEnd = Clock::now();

    outResult.width = width;
    outResult.height = height;
    outResult.sourceHash = ComputeSectorLightmapSourceHash(map);
    outResult.validChartTexels = static_cast<int>(bakeTexels.size());
    outResult.allocatedChartRectanglePixels = allocatedChartRectanglePixels;
    outResult.staticGeometryTriangles = static_cast<int>(triangles.size());
    outResult.bvhNodes = static_cast<int>(bvh.nodes.size());
    outResult.bvhLeaves = bvhStats.leafCount;
    outResult.bvhLeafTriangleLimit = kSectorLightmapBvhLeafTriangleCount;
    outResult.bvhAverageTrianglesPerLeaf = bvhStats.leafCount > 0
            ? static_cast<double>(bvhStats.totalLeafTriangles) / static_cast<double>(bvhStats.leafCount)
            : 0.0;
    outResult.bvhMaxTrianglesInLeaf = bvhStats.maxTrianglesInLeaf;
    outResult.staticLightCount = static_cast<int>(map.staticLights.size());
    outResult.directShadowRays = static_cast<long long>(stats.directHardShadow.raysCast);
    outResult.softShadowSourceRays = static_cast<long long>(stats.softShadowSource.raysCast);
    outResult.ambientOcclusionRays = static_cast<long long>(stats.ambientOcclusion.raysCast);
    outResult.indirectBounceRays = static_cast<long long>(stats.indirectBounce.raysCast);
    outResult.directHardShadowStats = stats.directHardShadow;
    outResult.softShadowSourceStats = stats.softShadowSource;
    outResult.ambientOcclusionStats = stats.ambientOcclusion;
    outResult.indirectBounceStats = stats.indirectBounce;
    outResult.bvhBuildSeconds = std::chrono::duration<double>(bvhBuildEnd - bvhBuildStart).count();
    outResult.directLightingSeconds = std::chrono::duration<double>(directEnd - directStart).count();
    outResult.ambientOcclusionSeconds = std::chrono::duration<double>(aoEnd - aoStart).count();
    outResult.indirectBounceSeconds = std::chrono::duration<double>(indirectEnd - indirectStart).count();
    outResult.gutterExportSeconds = std::chrono::duration<double>(exportEnd - exportStart).count();
    outResult.totalBakeSeconds = std::chrono::duration<double>(exportEnd - totalStart).count();
    return true;
}

bool BakeSectorLightmap(
        const SectorLightmapBakeInput& input,
        const SectorLightmapBakeCallbacks& callbacks,
        SectorLightmapBakeResult& outResult,
        std::string& outError)
{
    using Clock = std::chrono::steady_clock;
    const auto layoutStart = Clock::now();
    ReportProgress(callbacks, SectorLightmapBakePhase::BuildingLayout, 0, 1);

    SectorLightmapLayout layout;
    if (!BuildSectorLightmapLayout(input.mapSnapshot, layout, outError)) {
        return false;
    }
    ReportProgress(callbacks, SectorLightmapBakePhase::BuildingLayout, 1, 1);
    if (CheckBakeCancelled(callbacks, outError)) {
        return false;
    }

    const auto layoutEnd = Clock::now();
    if (!BakeSectorLightmap(input.mapSnapshot, layout, input.temporaryOutputPath.c_str(), callbacks, outResult, outError)) {
        return false;
    }

    outResult.layoutSeconds = std::chrono::duration<double>(layoutEnd - layoutStart).count();
    outResult.totalBakeSeconds += outResult.layoutSeconds;
    if (!input.expectedSourceHash.empty()) {
        outResult.sourceHash = input.expectedSourceHash;
    }
    return true;
}

std::string FormatSectorLightmapBakeReport(const SectorLightmapBakeResult& result)
{
    const double atlasPixels = static_cast<double>(result.width) * static_cast<double>(result.height);
    const double validAtlasOccupancy = atlasPixels > 0.0
            ? (static_cast<double>(result.validChartTexels) / atlasPixels) * 100.0
            : 0.0;
    const double chartRectangleOccupancy = atlasPixels > 0.0
            ? (static_cast<double>(result.allocatedChartRectanglePixels) / atlasPixels) * 100.0
            : 0.0;
    const double chartPayloadEfficiency = result.allocatedChartRectanglePixels > 0
            ? (static_cast<double>(result.validChartTexels) / static_cast<double>(result.allocatedChartRectanglePixels)) * 100.0
            : 0.0;
    const auto averageTriangleTestsPerRay = [](const SectorLightmapRaycastStats& stats) {
        return stats.raysCast > 0
                ? static_cast<double>(stats.triangleTests) / static_cast<double>(stats.raysCast)
                : 0.0;
    };
    const uint64_t totalRays = result.directHardShadowStats.raysCast
            + result.softShadowSourceStats.raysCast
            + result.ambientOcclusionStats.raysCast
            + result.indirectBounceStats.raysCast;
    const uint64_t totalTriangleTests = result.directHardShadowStats.triangleTests
            + result.softShadowSourceStats.triangleTests
            + result.ambientOcclusionStats.triangleTests
            + result.indirectBounceStats.triangleTests;
    const double totalAverageTriangleTestsPerRay = totalRays > 0
            ? static_cast<double>(totalTriangleTests) / static_cast<double>(totalRays)
            : 0.0;

    std::ostringstream report;
    report << "Lightmap bake report\n";
    report << TextFormat("  Atlas: %d x %d\n", result.width, result.height);
    report << TextFormat("  Atlas pixels: %llu\n", static_cast<unsigned long long>(static_cast<uint64_t>(result.width) * static_cast<uint64_t>(result.height)));
    report << TextFormat("  Valid chart texels: %d\n", result.validChartTexels);
    report << TextFormat("  Valid atlas occupancy: %.2f%%\n", validAtlasOccupancy);
    report << TextFormat("  Allocated chart rectangle pixels: %d\n", result.allocatedChartRectanglePixels);
    report << TextFormat("  Chart rectangle occupancy: %.2f%%\n", chartRectangleOccupancy);
    report << TextFormat("  Chart payload efficiency: %.2f%%\n", chartPayloadEfficiency);
    report << TextFormat("  Static geometry triangles: %d\n", result.staticGeometryTriangles);
    report << TextFormat("  BVH nodes: %d\n", result.bvhNodes);
    report << TextFormat("  BVH leaves: %d\n", result.bvhLeaves);
    report << TextFormat("  BVH leaf triangle limit: %d\n", result.bvhLeafTriangleLimit);
    report << TextFormat("  Average triangles per leaf: %.2f\n", result.bvhAverageTrianglesPerLeaf);
    report << TextFormat("  Max triangles in leaf: %d\n", result.bvhMaxTrianglesInLeaf);
    report << TextFormat("  Static lights: %d\n\n", result.staticLightCount);
    auto appendRayStats = [&](const char* label, const SectorLightmapRaycastStats& stats) {
        report << TextFormat("  %s: %llu\n", label, static_cast<unsigned long long>(stats.raysCast));
        report << TextFormat("    AABB tests: %llu\n", static_cast<unsigned long long>(stats.aabbTests));
        report << TextFormat("    AABB hits: %llu\n", static_cast<unsigned long long>(stats.aabbHits));
        report << TextFormat("    Triangle tests: %llu\n", static_cast<unsigned long long>(stats.triangleTests));
        report << TextFormat("    Triangle hits: %llu\n", static_cast<unsigned long long>(stats.triangleHits));
        report << TextFormat("    Average triangle tests/ray: %.2f\n", averageTriangleTestsPerRay(stats));
    };
    appendRayStats("Direct hard-shadow rays", result.directHardShadowStats);
    report << "\n";
    appendRayStats("Soft-shadow source rays", result.softShadowSourceStats);
    report << "\n";
    appendRayStats("AO rays", result.ambientOcclusionStats);
    report << "\n";
    appendRayStats("Indirect bounce rays", result.indirectBounceStats);
    report << "\n";
    report << TextFormat("  Total rays: %llu\n", static_cast<unsigned long long>(totalRays));
    report << TextFormat("  Total triangle tests: %llu\n", static_cast<unsigned long long>(totalTriangleTests));
    report << TextFormat("  Average triangle tests/ray: %.2f\n\n", totalAverageTriangleTestsPerRay);
    report << TextFormat("  Layout: %.2fs\n", result.layoutSeconds);
    report << TextFormat("  BVH build: %.2fs\n", result.bvhBuildSeconds);
    report << TextFormat("  Direct lighting: %.2fs\n", result.directLightingSeconds);
    report << TextFormat("  AO: %.2fs\n", result.ambientOcclusionSeconds);
    report << TextFormat("  Indirect bounce: %.2fs\n", result.indirectBounceSeconds);
    report << TextFormat("  Gutter dilation/export: %.2fs\n", result.gutterExportSeconds);
    report << TextFormat("  Total bake: %.2fs", result.totalBakeSeconds);
    return report.str();
}

void PrintSectorLightmapBakeReport(const SectorLightmapBakeResult& result)
{
    const std::string report = FormatSectorLightmapBakeReport(result);
    std::istringstream stream(report);
    std::string line;
    while (std::getline(stream, line)) {
        TraceLog(LOG_INFO, "%s", line.c_str());
    }
}

std::string ComputeSectorLightmapSourceHash(const SectorMap& map)
{
    uint64_t hash = 14695981039346656037ull;
    FnvAppendString(hash, "sector-lightmap");
    FnvAppendInt(hash, kSectorLightmapBakeVersion);
    FnvAppendInt(hash, SectorLightmapAtlasWidth);
    FnvAppendInt(hash, SectorLightmapAtlasHeight);
    FnvAppendInt(hash, SectorLightmapGutterTexels);
    FnvAppendFloat(hash, SectorLightmapTexelsPerWorldUnit);
    FnvAppendInt(hash, kDirectSoftShadowSampleCount);
    FnvAppendInt(hash, kAmbientOcclusionSampleCount);
    FnvAppendInt(hash, kIndirectBounceSampleCount);
    FnvAppendFloat(hash, kNeutralBounceAlbedo);
    FnvAppendFloat(hash, std::clamp(map.lightmapSettings.ambientOcclusionRadius, 0.05f, 16.0f));
    FnvAppendFloat(hash, std::clamp(map.lightmapSettings.ambientOcclusionStrength, 0.0f, 1.0f));
    FnvAppendFloat(hash, std::clamp(map.lightmapSettings.indirectBounceRadius, 0.05f, 16.0f));
    FnvAppendFloat(hash, std::clamp(map.lightmapSettings.indirectBounceStrength, 0.0f, 1.0f));

    FnvAppendInt(hash, static_cast<int>(map.sectors.size()));
    for (const SectorDefinition& sector : map.sectors) {
        FnvAppendInt(hash, static_cast<int>(sector.points.size()));
        for (SectorPoint point : sector.points) {
            FnvAppendFloat(hash, point.x);
            FnvAppendFloat(hash, point.y);
        }
        FnvAppendFloat(hash, sector.floorZ);
        FnvAppendFloat(hash, sector.ceilingZ);
    }

    FnvAppendInt(hash, static_cast<int>(map.staticLights.size()));
    for (const SectorStaticPointLight& light : map.staticLights) {
        FnvAppendFloat(hash, light.position.x);
        FnvAppendFloat(hash, light.position.y);
        FnvAppendFloat(hash, light.position.z);
        FnvAppendInt(hash, static_cast<int>(light.color.r));
        FnvAppendInt(hash, static_cast<int>(light.color.g));
        FnvAppendInt(hash, static_cast<int>(light.color.b));
        FnvAppendFloat(hash, light.intensity);
        FnvAppendFloat(hash, light.radius);
        FnvAppendFloat(hash, std::min(std::clamp(light.sourceRadius, 0.0f, 8.0f), light.radius * 0.5f));
    }

    return HashToString(hash);
}

SectorLightmapStatus GetSectorLightmapStatus(const SectorMap& map)
{
    if (map.bakedLightmap.path.empty()
            || map.bakedLightmap.width <= 0
            || map.bakedLightmap.height <= 0
            || map.bakedLightmap.sourceHash.empty()) {
        return SectorLightmapStatus::None;
    }

    if (map.bakedLightmap.sourceHash != ComputeSectorLightmapSourceHash(map)) {
        return SectorLightmapStatus::Stale;
    }

    if (!FileExistsResolved(ResolveSectorAssetPath(map.bakedLightmap.path))) {
        return SectorLightmapStatus::Stale;
    }

    return SectorLightmapStatus::Valid;
}

const char* SectorLightmapStatusText(SectorLightmapStatus status)
{
    switch (status) {
        case SectorLightmapStatus::None: return "No baked lightmap";
        case SectorLightmapStatus::Valid: return "Lightmap valid";
        case SectorLightmapStatus::Stale: return "Lightmap stale - rebake required";
    }
    return "No baked lightmap";
}

} // namespace game
