#include "sector_demo/SectorLightmap.h"

#include "sector_demo/SectorColor.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorMath.h"
#include "sector_demo/SectorRectLight.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace game {

SectorLightmapBakeQualityParameters ResolveSectorLightmapBakeQuality(
        SectorLightmapBakeQualityPreset preset)
{
    switch (NormalizeSectorLightmapBakeQualityPreset(preset)) {
        case SectorLightmapBakeQualityPreset::Draft:
            return SectorLightmapBakeQualityParameters{4.0f, 4, 6, 4};
        case SectorLightmapBakeQualityPreset::High:
            return SectorLightmapBakeQualityParameters{16.0f, 12, 18, 12};
        case SectorLightmapBakeQualityPreset::Standard:
            return SectorLightmapBakeQualityParameters{
                    SectorLightmapTexelsPerWorldUnit,
                    kDirectSoftShadowSampleCount,
                    kAmbientOcclusionSampleCount,
                    kIndirectBounceSampleCount};
    }
    return SectorLightmapBakeQualityParameters{};
}

const char* SectorLightmapBakeQualityPresetName(
        SectorLightmapBakeQualityPreset preset)
{
    switch (NormalizeSectorLightmapBakeQualityPreset(preset)) {
        case SectorLightmapBakeQualityPreset::Draft:
            return "Draft";
        case SectorLightmapBakeQualityPreset::High:
            return "High";
        case SectorLightmapBakeQualityPreset::Standard:
            return "Standard";
    }
    return "Standard";
}

namespace {

struct BakeTriangle {
    Vector3 worldPosition0 = {};
    Vector3 worldPosition1 = {};
    Vector3 worldPosition2 = {};
    Vector3 normal = {};
    Vector2 lightmapUv0 = {};
    Vector2 lightmapUv1 = {};
    Vector2 lightmapUv2 = {};
    int lightmapAtlasIndex = -1;
    SectorGeneratedSurfaceRef surfaceRef;
    int sourceSurfaceIndex = -1;
    int triangleIndex = -1;
    bool castsDirectShadow = true;
};

struct RasterHit {
    bool hit = false;
    Vector3 position = {};
    Vector3 normal = {};
    Vector3 geometricNormal = {};
    Vector2 uv = {};
    int triangleIndex = -1;
};

struct RayHit {
    bool hit = false;
    float distance = 0.0f;
    Vector3 normal = {};
    Vector2 lightmapUv = {};
    int lightmapAtlasIndex = -1;
    int sourceSurfaceIndex = -1;
    int triangleIndex = -1;
    float barycentric0 = 0.0f;
    float barycentric1 = 0.0f;
    float barycentric2 = 0.0f;
};

struct AlphaRayHit {
    bool hit = false;
    float distance = 0.0f;
    Vector2 uv = {};
    std::string materialId;
    float alphaCutoff = 0.5f;
};

struct BakeTexel {
    int atlasIndex = -1;
    int x = 0;
    int y = 0;
    size_t pixelIndex = 0;
    SectorGeneratedSurfaceRef surfaceRef;
    int sourceSurfaceIndex = -1;
    int triangleIndex = -1;
    Vector3 position = {};
    Vector3 normal = {};
    Vector3 geometricNormal = {};
};

struct LightmapWorldPointLight {
    Vector3 position = {};
    Vector3 linearColor = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 0.0f;
    float sourceRadius = 0.0f;
    bool castsShadow = true;
};

struct LightmapWorldSpotLight {
    Vector3 position = {};
    Vector3 target = {};
    Vector3 linearColor = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 0.0f;
    float sourceRadius = 0.0f;
    float innerConeDegrees = 0.0f;
    float outerConeDegrees = 0.0f;
    bool castsShadow = true;
};

struct LightmapWorldRectLight {
    Vector3 position = {};
    SectorRectLightBasis basis;
    Vector3 linearColor = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float startFeather = 0.0f;
    bool castsShadow = true;
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

struct LightmapWorldDirectionalLight {
    bool enabled = false;
    Vector3 directionToLight = {};
    Vector3 linearColor = {1.0f, 1.0f, 1.0f};
    float intensity = 0.0f;
};

constexpr float BakeEpsilon = 0.0001f;
constexpr float RayOriginEpsilon = 0.01f;
constexpr float RayHitEpsilon = 0.001f;
constexpr float BvhAabbEpsilon = 0.00001f;
constexpr int kSectorLightmapBvhLeafTriangleCount = 4;
constexpr int kSectorLightmapBvhTraversalStackSize = 128;
constexpr int kSectorLightmapAlphaOcclusionIterationLimit = 64;
constexpr uint32_t kSectorLightmapProgressChunk = 512;
constexpr float Pi = 3.14159265358979323846f;
constexpr char kObjectProbeSidecarMagic[4] = {'S', 'O', 'P', 'B'};
constexpr char kLightmapArtifactMagic[4] = {'S', 'L', 'M', 'H'};
constexpr uint32_t kLightmapArtifactFixedHeaderBytes = 52;
constexpr uint32_t kLightmapArtifactChannels = 8;
constexpr uint32_t kLightmapArtifactEncodingMixedRgba16Rgba8 = 2;
constexpr uint32_t kLightmapArtifactSemanticsHdrRgbAoDominantDirection = 2;
constexpr uint32_t kObjectProbeFixedHeaderBytes = 48;

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
    return ClampColorByte(value * 255.0f);
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

Vector2 Interpolate(Vector2 a, Vector2 b, Vector2 c, float wa, float wb, float wc)
{
    return Vector2{
            a.x * wa + b.x * wb + c.x * wc,
            a.y * wa + b.y * wb + c.y * wc
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
        outHit.geometricNormal = outHit.normal;
        outHit.uv = Interpolate(va.uv, vb.uv, vc.uv, wa, wb, wc);
        outHit.triangleIndex = static_cast<int>(i / 3);
        return true;
    }

    return false;
}

Vector3 TransformStaticModelPosition(
        Vector3 importedPosition,
        const SectorStaticModelLightmapObject& object)
{
    const Matrix authoredTransform = BuildSectorStaticModelAuthoredTransform(
            object.worldPosition,
            object.rotationXRadians,
            object.yawRadians,
            object.rotationZRadians,
            object.scale);
    return Vector3Transform(importedPosition, authoredTransform);
}

Vector3 TransformStaticModelNormal(
        Vector3 importedNormal,
        const SectorStaticModelLightmapObject& object)
{
    const Vector3 rotated = RotateSectorStaticModelDirection(
            importedNormal,
            object.rotationXRadians,
            object.yawRadians,
            object.rotationZRadians);
    return Vector3LengthSqr(rotated) > BakeEpsilon
            ? Vector3Normalize(rotated)
            : Vector3{0.0f, 1.0f, 0.0f};
}

bool RasterizeStaticModelMeshPoint(
        const SectorStaticModelLightmapMesh& mesh,
        const SectorStaticModelLightmapObject& object,
        Vector2 localUv,
        RasterHit& outHit)
{
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t ia = mesh.indices[i];
        const uint32_t ib = mesh.indices[i + 1];
        const uint32_t ic = mesh.indices[i + 2];
        if (ia >= mesh.localLightmapUvs.size()
                || ib >= mesh.localLightmapUvs.size()
                || ic >= mesh.localLightmapUvs.size()
                || ia >= mesh.importedPositions.size()
                || ib >= mesh.importedPositions.size()
                || ic >= mesh.importedPositions.size()
                || ia >= mesh.importedNormals.size()
                || ib >= mesh.importedNormals.size()
                || ic >= mesh.importedNormals.size()) {
            continue;
        }
        float wa = 0.0f;
        float wb = 0.0f;
        float wc = 0.0f;
        if (!Barycentric(
                    localUv,
                    mesh.localLightmapUvs[ia],
                    mesh.localLightmapUvs[ib],
                    mesh.localLightmapUvs[ic],
                    wa,
                    wb,
                    wc)) {
            continue;
        }
        outHit.hit = true;
        outHit.position = TransformStaticModelPosition(
                Interpolate(
                        mesh.importedPositions[ia],
                        mesh.importedPositions[ib],
                        mesh.importedPositions[ic],
                        wa,
                        wb,
                        wc),
                object);
        outHit.normal = TransformStaticModelNormal(
                Vector3Normalize(Interpolate(
                        mesh.importedNormals[ia],
                        mesh.importedNormals[ib],
                        mesh.importedNormals[ic],
                        wa,
                        wb,
                        wc)),
                object);
        outHit.geometricNormal = outHit.normal;
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

bool RayIntersectsTriangle(
        Vector3 origin,
        Vector3 direction,
        const SectorLightmapAlphaOccluderTriangle& tri,
        float maxDistance,
        float& outDistance,
        float& outBarycentric0,
        float& outBarycentric1,
        float& outBarycentric2)
{
    BakeTriangle bakeTri;
    bakeTri.worldPosition0 = tri.worldPosition0;
    bakeTri.worldPosition1 = tri.worldPosition1;
    bakeTri.worldPosition2 = tri.worldPosition2;
    return RayIntersectsTriangle(
            origin,
            direction,
            bakeTri,
            maxDistance,
            outDistance,
            outBarycentric0,
            outBarycentric1,
            outBarycentric2);
}

uint32_t FloatToLittleEndianBits(float value)
{
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float32 sidecar fields must be 32 bits");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float FloatFromLittleEndianBits(uint32_t bits)
{
    float value = 0.0f;
    static_assert(sizeof(bits) == sizeof(value), "float32 sidecar fields must be 32 bits");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool WriteU32LE(std::ostream& output, uint32_t value)
{
    const char bytes[4] = {
            static_cast<char>(value & 0xffu),
            static_cast<char>((value >> 8u) & 0xffu),
            static_cast<char>((value >> 16u) & 0xffu),
            static_cast<char>((value >> 24u) & 0xffu)};
    output.write(bytes, sizeof(bytes));
    return output.good();
}

bool WriteU16LE(std::ostream& output, uint16_t value)
{
    const char bytes[2] = {
            static_cast<char>(value & 0xffu),
            static_cast<char>((value >> 8u) & 0xffu)};
    output.write(bytes, sizeof(bytes));
    return output.good();
}

bool WriteU64LE(std::ostream& output, uint64_t value)
{
    for (unsigned int shift = 0; shift < 64; shift += 8) {
        output.put(static_cast<char>((value >> shift) & 0xffu));
    }
    return output.good();
}

bool WriteI32LE(std::ostream& output, int32_t value)
{
    return WriteU32LE(output, static_cast<uint32_t>(value));
}

bool WriteF32LE(std::ostream& output, float value)
{
    return WriteU32LE(output, FloatToLittleEndianBits(value));
}

bool ReadU32LE(std::istream& input, uint32_t& outValue)
{
    unsigned char bytes[4] = {};
    input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(bytes))) {
        return false;
    }

    outValue = static_cast<uint32_t>(bytes[0])
            | (static_cast<uint32_t>(bytes[1]) << 8u)
            | (static_cast<uint32_t>(bytes[2]) << 16u)
            | (static_cast<uint32_t>(bytes[3]) << 24u);
    return true;
}

bool ReadU16LE(std::istream& input, uint16_t& outValue)
{
    unsigned char bytes[2] = {};
    input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(bytes))) {
        return false;
    }
    outValue = static_cast<uint16_t>(bytes[0])
            | static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8u);
    return true;
}

bool ReadU64LE(std::istream& input, uint64_t& outValue)
{
    unsigned char bytes[8] = {};
    input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(bytes))) {
        return false;
    }
    outValue = 0;
    for (unsigned int index = 0; index < 8; ++index) {
        outValue |= static_cast<uint64_t>(bytes[index]) << (index * 8u);
    }
    return true;
}

uint64_t Fnv1aBytes(const std::vector<uint8_t>& bytes)
{
    uint64_t hash = 14695981039346656037ull;
    for (const uint8_t byte : bytes) {
        hash = FnvAppendByte(hash, byte);
    }
    return hash;
}

bool StatisticsInitialized(const SectorIlluminationStatistics& statistics)
{
    return statistics.sampleCount != 0;
}

void AccumulateStatistics(
        SectorIlluminationStatistics& statistics,
        Vector3 rgb,
        float auxiliary)
{
    if (!StatisticsInitialized(statistics)) {
        statistics.rgbMin = rgb;
        statistics.rgbMax = rgb;
        statistics.auxiliaryMin = auxiliary;
        statistics.auxiliaryMax = auxiliary;
    } else {
        statistics.rgbMin = Vector3{
                std::min(statistics.rgbMin.x, rgb.x),
                std::min(statistics.rgbMin.y, rgb.y),
                std::min(statistics.rgbMin.z, rgb.z)};
        statistics.rgbMax = Vector3{
                std::max(statistics.rgbMax.x, rgb.x),
                std::max(statistics.rgbMax.y, rgb.y),
                std::max(statistics.rgbMax.z, rgb.z)};
        statistics.auxiliaryMin = std::min(statistics.auxiliaryMin, auxiliary);
        statistics.auxiliaryMax = std::max(statistics.auxiliaryMax, auxiliary);
    }
    statistics.rgbChannelsAboveOne += static_cast<uint64_t>(rgb.x > 1.0f)
            + static_cast<uint64_t>(rgb.y > 1.0f)
            + static_cast<uint64_t>(rgb.z > 1.0f);
    ++statistics.sampleCount;
}

void MergeStatistics(
        SectorIlluminationStatistics& aggregate,
        const SectorIlluminationStatistics& value)
{
    if (value.sampleCount == 0) {
        return;
    }
    if (aggregate.sampleCount == 0) {
        aggregate = value;
        return;
    }
    aggregate.rgbMin = Vector3{
            std::min(aggregate.rgbMin.x, value.rgbMin.x),
            std::min(aggregate.rgbMin.y, value.rgbMin.y),
            std::min(aggregate.rgbMin.z, value.rgbMin.z)};
    aggregate.rgbMax = Vector3{
            std::max(aggregate.rgbMax.x, value.rgbMax.x),
            std::max(aggregate.rgbMax.y, value.rgbMax.y),
            std::max(aggregate.rgbMax.z, value.rgbMax.z)};
    aggregate.auxiliaryMin = std::min(aggregate.auxiliaryMin, value.auxiliaryMin);
    aggregate.auxiliaryMax = std::max(aggregate.auxiliaryMax, value.auxiliaryMax);
    aggregate.sampleCount += value.sampleCount;
    aggregate.rgbChannelsAboveOne += value.rgbChannelsAboveOne;
}

bool ReadI32LE(std::istream& input, int32_t& outValue)
{
    uint32_t value = 0;
    if (!ReadU32LE(input, value)) {
        return false;
    }
    outValue = static_cast<int32_t>(value);
    return true;
}

bool ReadF32LE(std::istream& input, float& outValue)
{
    uint32_t bits = 0;
    if (!ReadU32LE(input, bits)) {
        return false;
    }
    outValue = FloatFromLittleEndianBits(bits);
    return true;
}

bool WriteProbeVector(std::ostream& output, Vector3 value)
{
    return WriteF32LE(output, value.x)
            && WriteF32LE(output, value.y)
            && WriteF32LE(output, value.z);
}

bool ReadProbeVector(std::istream& input, Vector3& outValue)
{
    return ReadF32LE(input, outValue.x)
            && ReadF32LE(input, outValue.y)
            && ReadF32LE(input, outValue.z);
}

bool AppendLoopPolygon(
        const SectorTopologyMap& map,
        const SectorTopologyLoop& loop,
        std::vector<SectorTopologyCoordPoint>& outPolygon,
        std::string& outError)
{
    outPolygon.clear();
    outPolygon.reserve(loop.vertexIds.size());
    for (const int vertexId : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
        if (vertex == nullptr) {
            outError = "Object probe placement failed: missing topology vertex "
                    + std::to_string(vertexId);
            return false;
        }
        outPolygon.push_back(SectorTopologyCoordPoint{vertex->x, vertex->y});
    }
    if (outPolygon.size() < 3) {
        outError = "Object probe placement failed: topology loop has fewer than three vertices";
        return false;
    }
    return true;
}

bool IsStrictlyInsideProbePolygon(
        const std::vector<SectorTopologyCoordPoint>& outer,
        const std::vector<std::vector<SectorTopologyCoordPoint>>& holes,
        SectorTopologyCoordPoint point)
{
    if (SectorTopologyClassifyPointInPolygon(outer, point) != SectorTopologyPointContainment::Inside) {
        return false;
    }
    for (const std::vector<SectorTopologyCoordPoint>& hole : holes) {
        if (SectorTopologyClassifyPointInPolygon(hole, point) != SectorTopologyPointContainment::Outside) {
            return false;
        }
    }
    return true;
}

double ProbePointSegmentDistanceSquared(
        SectorTopologyCoordPoint point,
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b)
{
    const double segmentX = static_cast<double>(b.x) - static_cast<double>(a.x);
    const double segmentY = static_cast<double>(b.y) - static_cast<double>(a.y);
    const double pointX = static_cast<double>(point.x) - static_cast<double>(a.x);
    const double pointY = static_cast<double>(point.y) - static_cast<double>(a.y);
    const double lengthSquared = segmentX * segmentX + segmentY * segmentY;
    if (!(lengthSquared > 0.0) || !std::isfinite(lengthSquared)) {
        return pointX * pointX + pointY * pointY;
    }

    const double t = std::clamp(
            (pointX * segmentX + pointY * segmentY) / lengthSquared,
            0.0,
            1.0);
    const double closestX = static_cast<double>(a.x) + t * segmentX;
    const double closestY = static_cast<double>(a.y) + t * segmentY;
    const double dx = static_cast<double>(point.x) - closestX;
    const double dy = static_cast<double>(point.y) - closestY;
    return dx * dx + dy * dy;
}

bool HasProbeLoopBoundaryClearance(
        const std::vector<SectorTopologyCoordPoint>& loop,
        SectorTopologyCoordPoint point,
        double clearanceSquared)
{
    for (size_t index = 0; index < loop.size(); ++index) {
        const SectorTopologyCoordPoint a = loop[index];
        const SectorTopologyCoordPoint b = loop[(index + 1) % loop.size()];
        if (ProbePointSegmentDistanceSquared(point, a, b) < clearanceSquared) {
            return false;
        }
    }
    return true;
}

bool IsValidProbePolygonPoint(
        const std::vector<SectorTopologyCoordPoint>& outer,
        const std::vector<std::vector<SectorTopologyCoordPoint>>& holes,
        SectorTopologyCoordPoint point,
        double boundaryClearanceCoord)
{
    if (!IsStrictlyInsideProbePolygon(outer, holes, point)) {
        return false;
    }

    const double clearanceSquared = boundaryClearanceCoord * boundaryClearanceCoord;
    if (!HasProbeLoopBoundaryClearance(outer, point, clearanceSquared)) {
        return false;
    }
    for (const std::vector<SectorTopologyCoordPoint>& hole : holes) {
        if (!HasProbeLoopBoundaryClearance(hole, point, clearanceSquared)) {
            return false;
        }
    }
    return true;
}

SectorTopologyCoordPoint ProbePolygonAabbCenter(
        SectorCoord minX,
        SectorCoord minY,
        SectorCoord maxX,
        SectorCoord maxY)
{
    return SectorTopologyCoordPoint{
            static_cast<SectorCoord>(
                    (static_cast<int64_t>(minX) + static_cast<int64_t>(maxX)) / 2),
            static_cast<SectorCoord>(
                    (static_cast<int64_t>(minY) + static_cast<int64_t>(maxY)) / 2)};
}

bool FindRepresentativeProbePoint(
        const std::vector<SectorTopologyCoordPoint>& outer,
        const std::vector<std::vector<SectorTopologyCoordPoint>>& holes,
        SectorCoord minX,
        SectorCoord minY,
        SectorCoord maxX,
        SectorCoord maxY,
        double boundaryClearanceCoord,
        SectorTopologyCoordPoint& outPoint)
{
    const SectorTopologyCoordPoint center = ProbePolygonAabbCenter(minX, minY, maxX, maxY);
    if (IsValidProbePolygonPoint(
                outer, holes, center, boundaryClearanceCoord)) {
        outPoint = center;
        return true;
    }

    int64_t sumX = 0;
    int64_t sumY = 0;
    for (const SectorTopologyCoordPoint point : outer) {
        sumX += point.x;
        sumY += point.y;
    }
    if (!outer.empty()) {
        const SectorTopologyCoordPoint centroid{
                static_cast<SectorCoord>(sumX / static_cast<int64_t>(outer.size())),
                static_cast<SectorCoord>(sumY / static_cast<int64_t>(outer.size()))};
        if (IsValidProbePolygonPoint(
                    outer, holes, centroid, boundaryClearanceCoord)) {
            outPoint = centroid;
            return true;
        }
    }

    constexpr int fallbackDivisions = 16;
    for (int yStep = 0; yStep < fallbackDivisions; ++yStep) {
        for (int xStep = 0; xStep < fallbackDivisions; ++xStep) {
            const int64_t x = static_cast<int64_t>(minX)
                    + ((static_cast<int64_t>(maxX) - static_cast<int64_t>(minX)) * (2 * xStep + 1))
                            / (2 * fallbackDivisions);
            const int64_t y = static_cast<int64_t>(minY)
                    + ((static_cast<int64_t>(maxY) - static_cast<int64_t>(minY)) * (2 * yStep + 1))
                            / (2 * fallbackDivisions);
            const SectorTopologyCoordPoint candidate{
                    static_cast<SectorCoord>(x),
                    static_cast<SectorCoord>(y)};
            if (IsValidProbePolygonPoint(
                        outer, holes, candidate, boundaryClearanceCoord)) {
                outPoint = candidate;
                return true;
            }
        }
    }

    return false;
}

struct ResolvedObjectProbeLayerHeights {
    float lowerWorld = 0.0f;
    float upperWorld = 0.0f;
    bool hasUpperLayer = false;
};

ResolvedObjectProbeLayerHeights ResolveObjectProbeLayerHeights(
        const SectorTopologySector& sector,
        const SectorBakedObjectLightProbePlacementSettings& settings,
        std::vector<SectorBakedObjectLightProbePlacementDiagnostic>* diagnostics)
{
    const float floorWorld = SectorAuthoringToWorldDistance(sector.floorZ);
    const float ceilingWorld = SectorAuthoringToWorldDistance(sector.ceilingZ);
    const float clearHeight = ceilingWorld - floorWorld;
    const bool fitsTwoLayers = clearHeight > kObjectProbeSurfaceClearanceWorld * 2.0f
            && settings.lowerHeightWorld >= kObjectProbeSurfaceClearanceWorld
            && settings.upperHeightWorld <= clearHeight - kObjectProbeSurfaceClearanceWorld
            && settings.upperHeightWorld - settings.lowerHeightWorld
                    >= kObjectProbeMinimumLayerSeparationWorld;
    if (fitsTwoLayers) {
        return ResolvedObjectProbeLayerHeights{
                floorWorld + settings.lowerHeightWorld,
                floorWorld + settings.upperHeightWorld,
                true};
    }

    const bool configuredSingleLayer =
            std::fabs(settings.upperHeightWorld - settings.lowerHeightWorld)
                    <= BakeEpsilon
            && settings.lowerHeightWorld >= kObjectProbeSurfaceClearanceWorld
            && settings.lowerHeightWorld
                    <= clearHeight - kObjectProbeSurfaceClearanceWorld;
    if (configuredSingleLayer) {
        const float heightWorld = floorWorld + settings.lowerHeightWorld;
        if (diagnostics != nullptr) {
            diagnostics->push_back(SectorBakedObjectLightProbePlacementDiagnostic{
                    sector.id,
                    "Object probe placement used the configured single layer"});
        }
        return ResolvedObjectProbeLayerHeights{
                heightWorld,
                heightWorld,
                false};
    }

    const float midpoint = ceilingWorld > floorWorld
            ? (floorWorld + ceilingWorld) * 0.5f
            : floorWorld + settings.lowerHeightWorld;
    if (diagnostics != nullptr) {
        diagnostics->push_back(SectorBakedObjectLightProbePlacementDiagnostic{
                sector.id,
                "Object probe placement used one midpoint layer because the sector cannot safely fit both configured heights"});
    }
    return ResolvedObjectProbeLayerHeights{midpoint, midpoint, false};
}

Vector3 MakeProbeWorldPosition(
        SectorTopologyCoordPoint point,
        float heightWorld)
{
    return Vector3{
            SectorCoordToWorldDistance(point.x),
            heightWorld,
            SectorCoordToWorldDistance(point.y)};
}

void AppendObjectProbeLayers(
        std::vector<SectorBakedObjectLightProbe>& probes,
        int sectorId,
        SectorTopologyCoordPoint point,
        const ResolvedObjectProbeLayerHeights& heights)
{
    SectorBakedObjectLightProbe lower;
    lower.sectorId = sectorId;
    lower.layer = SectorBakedObjectLightProbeLayer::Lower;
    lower.position = MakeProbeWorldPosition(point, heights.lowerWorld);
    probes.push_back(lower);
    if (heights.hasUpperLayer) {
        SectorBakedObjectLightProbe upper;
        upper.sectorId = sectorId;
        upper.layer = SectorBakedObjectLightProbeLayer::Upper;
        upper.position = MakeProbeWorldPosition(point, heights.upperWorld);
        probes.push_back(upper);
    }
}

float BvhSceneDiagonalWithMargin(const SectorLightmapBvh& bvh)
{
    if (bvh.nodes.empty()) {
        return 0.0f;
    }
    const BakeAabb& bounds = bvh.nodes.front().bounds;
    const Vector3 extent = Vector3Subtract(bounds.max, bounds.min);
    return std::max(0.0f, Vector3Length(extent) + RayOriginEpsilon * 4.0f);
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
        if (!IsFiniteVector3(tri.worldPosition0) || !IsFiniteVector3(tri.worldPosition1) || !IsFiniteVector3(tri.worldPosition2)) {
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

bool CastsLightmapOcclusion(const SectorGeneratedSurface& surface)
{
    return surface.castsLightmapOcclusion
            && surface.ref.kind != SectorGeneratedSurfaceKind::Middle;
}

bool CastsAlphaTestLightmapOcclusion(const SectorGeneratedSurface& surface)
{
    return surface.ref.kind == SectorGeneratedSurfaceKind::Middle
            && surface.alphaTest
            && !surface.materialId.empty();
}

bool IntersectRayAabb(const Ray& ray, const BakeAabb& bounds, float maxDistance, float& outEntryDistance)
{
    if (maxDistance <= 0.0f || !std::isfinite(maxDistance)
            || !IsFiniteVector3(ray.position) || !IsFiniteVector3(ray.direction)
            || !IsFiniteVector3(bounds.min) || !IsFiniteVector3(bounds.max)) {
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

bool IsExactSourceTriangle(const BakeTriangle& tri, int sourceSurfaceIndex, int sourceTriangleIndex)
{
    return tri.sourceSurfaceIndex == sourceSurfaceIndex && tri.triangleIndex == sourceTriangleIndex;
}

bool ShouldIgnoreBakeTriangle(
        const BakeTriangle& tri,
        const SectorGeneratedSurfaceRef& sourceSurfaceRef,
        int sourceSurfaceIndex,
        int sourceTriangleIndex)
{
    return IsExactSourceTriangle(tri, sourceSurfaceIndex, sourceTriangleIndex)
            || IsSameLogicalSectorLightmapSurface(tri.surfaceRef, sourceSurfaceRef);
}

bool ShouldIgnoreAlphaOccluderTriangle(
        const SectorLightmapAlphaOccluderTriangle& tri,
        const SectorGeneratedSurfaceRef& sourceSurfaceRef,
        int sourceSurfaceIndex,
        int sourceTriangleIndex)
{
    return (tri.sourceSurfaceIndex == sourceSurfaceIndex && tri.triangleIndex == sourceTriangleIndex)
            || IsSameLogicalSectorLightmapSurface(tri.surfaceRef, sourceSurfaceRef);
}

bool RaycastBakeTrianglesAnyHit(
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const Ray& ray,
        float maxDistance,
        const SectorGeneratedSurfaceRef& sourceSurfaceRef,
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
                if (!tri.castsDirectShadow
                        || IsExactSourceTriangle(
                                tri,
                                sourceSurfaceIndex,
                                sourceTriangleIndex)) {
                    continue;
                }
                if (stats != nullptr) {
                    ++stats->triangleTests;
                }
                if (RayIntersectsTriangle(ray.position, ray.direction, tri, maxDistance)) {
                    if (ShouldIgnoreBakeTriangle(tri, sourceSurfaceRef, sourceSurfaceIndex, sourceTriangleIndex)) {
                        if (stats != nullptr) {
                            ++stats->logicalSelfHitsIgnored;
                        }
                        continue;
                    }
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
        const SectorGeneratedSurfaceRef& sourceSurfaceRef,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        bool directShadowOnly,
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
                if ((directShadowOnly && !tri.castsDirectShadow)
                        || IsExactSourceTriangle(
                                tri,
                                sourceSurfaceIndex,
                                sourceTriangleIndex)) {
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
                    if (ShouldIgnoreBakeTriangle(tri, sourceSurfaceRef, sourceSurfaceIndex, sourceTriangleIndex)) {
                        if (stats != nullptr) {
                            ++stats->logicalSelfHitsIgnored;
                        }
                        continue;
                    }
                    if (stats != nullptr) {
                        ++stats->triangleHits;
                    }
                    closest.hit = true;
                    closest.distance = distance;
                    closest.normal = tri.normal;
                    closest.sourceSurfaceIndex = tri.sourceSurfaceIndex;
                    closest.triangleIndex = tri.triangleIndex;
                    closest.barycentric0 = barycentric0;
                    closest.barycentric1 = barycentric1;
                    closest.barycentric2 = barycentric2;
                    closest.lightmapUv = Vector2{
                            tri.lightmapUv0.x * barycentric0 + tri.lightmapUv1.x * barycentric1 + tri.lightmapUv2.x * barycentric2,
                            tri.lightmapUv0.y * barycentric0 + tri.lightmapUv1.y * barycentric1 + tri.lightmapUv2.y * barycentric2
                    };
                    closest.lightmapAtlasIndex = tri.lightmapAtlasIndex;
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

AlphaRayHit RaycastAlphaOccludersClosest(
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        const Ray& ray,
        float maxDistance,
        const SectorGeneratedSurfaceRef& sourceSurfaceRef,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        SectorLightmapRaycastStats* stats)
{
    AlphaRayHit closest{};
    closest.distance = maxDistance;
    for (const SectorLightmapAlphaOccluderTriangle& tri : alphaOccluders) {
        if (ShouldIgnoreAlphaOccluderTriangle(tri, sourceSurfaceRef, sourceSurfaceIndex, sourceTriangleIndex)) {
            continue;
        }

        float distance = 0.0f;
        float barycentric0 = 0.0f;
        float barycentric1 = 0.0f;
        float barycentric2 = 0.0f;
        if (stats != nullptr) {
            ++stats->triangleTests;
        }
        if (!RayIntersectsTriangle(
                    ray.position,
                    ray.direction,
                    tri,
                    closest.distance,
                    distance,
                    barycentric0,
                    barycentric1,
                    barycentric2)) {
            continue;
        }

        if (stats != nullptr) {
            ++stats->triangleHits;
        }
        closest.hit = true;
        closest.distance = distance;
        closest.uv = Interpolate(tri.uv0, tri.uv1, tri.uv2, barycentric0, barycentric1, barycentric2);
        closest.materialId = tri.materialId;
        closest.alphaCutoff = tri.alphaCutoff;
    }
    return closest;
}

bool RaycastBakeOcclusionAlphaAware(
        const SectorTopologyMap& map,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        const Ray& ray,
        float maxDistance,
        const SectorGeneratedSurfaceRef& sourceSurfaceRef,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        SectorLightmapRaycastStats* stats)
{
    if (alphaOccluders.empty()) {
        return RaycastBakeTrianglesAnyHit(
                bvh,
                triangles,
                ray,
                maxDistance,
                sourceSurfaceRef,
                sourceSurfaceIndex,
                sourceTriangleIndex,
                stats);
    }

    Vector3 origin = ray.position;
    float remainingDistance = maxDistance;
    for (int i = 0; i < kSectorLightmapAlphaOcclusionIterationLimit; ++i) {
        const Ray currentRay{origin, ray.direction};
        const RayHit opaqueHit = RaycastBakeTrianglesClosest(
                bvh,
                triangles,
                currentRay,
                remainingDistance,
                sourceSurfaceRef,
                sourceSurfaceIndex,
                sourceTriangleIndex,
                true,
                stats);
        const AlphaRayHit alphaHit = RaycastAlphaOccludersClosest(
                alphaOccluders,
                currentRay,
                remainingDistance,
                sourceSurfaceRef,
                sourceSurfaceIndex,
                sourceTriangleIndex,
                stats);

        if (!opaqueHit.hit && !alphaHit.hit) {
            return false;
        }
        if (opaqueHit.hit && (!alphaHit.hit || opaqueHit.distance <= alphaHit.distance)) {
            return true;
        }

        const SectorLightmapAlphaSample alphaSample =
                alphaMaskCache.Sample(map, alphaHit.materialId, alphaHit.uv, alphaHit.alphaCutoff);
        if (alphaSample.opaque) {
            return true;
        }

        const float stepDistance = alphaHit.distance + RayHitEpsilon * 2.0f;
        if (stepDistance >= remainingDistance) {
            return false;
        }
        origin = Vector3Add(origin, Vector3Scale(ray.direction, stepDistance));
        remainingDistance -= stepDistance;
    }

    TraceLog(LOG_WARNING, "Sector lightmap alpha occlusion ray exceeded transparent-hit iteration limit");
    return true;
}

bool IsOccluded(
        const SectorTopologyMap& map,
        Vector3 position,
        Vector3 normal,
        Vector3 lightPosition,
        const SectorGeneratedSurfaceRef& sourceSurfaceRef,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
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

    return RaycastBakeOcclusionAlphaAware(
            map,
            alphaMaskCache,
            bvh,
            triangles,
            alphaOccluders,
            ray,
            maxDistance,
            sourceSurfaceRef,
            sourceSurfaceIndex,
            sourceTriangleIndex,
            softShadowSample ? &stats.softShadowSource : &stats.directHardShadow
    );
}

RayHit TraceRay(
        Vector3 origin,
        Vector3 direction,
        float maxDistance,
        const SectorGeneratedSurfaceRef& sourceSurfaceRef,
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
            sourceSurfaceRef,
            sourceSurfaceIndex,
            sourceTriangleIndex,
            false,
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

Vector3 GeometricNormalForHit(const RasterHit& hit)
{
    return Vector3LengthSqr(hit.geometricNormal) > BakeEpsilon
            ? hit.geometricNormal
            : hit.normal;
}

struct DirectLightEvaluation {
    Vector3 radiance = {};
    Vector3 directionMoment = {};
};

float LinearLuminance(Vector3 value)
{
    return value.x * 0.2126f + value.y * 0.7152f + value.z * 0.0722f;
}

DirectLightEvaluation MakeDirectLightEvaluation(
        Vector3 radiance,
        Vector3 directionToLight)
{
    return DirectLightEvaluation{
            radiance,
            Vector3Scale(directionToLight, std::max(LinearLuminance(radiance), 0.0f))};
}

DirectLightEvaluation EvaluateDirectLightSample(
        const SectorTopologyMap& map,
        const LightmapWorldPointLight& light,
        Vector3 lightPosition,
        const RasterHit& hit,
        const SectorGeneratedSurfaceRef& surfaceRef,
        int surfaceIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        bool softShadowSample,
        BakeRayStats& stats)
{
    const Vector3 toLight = Vector3Subtract(lightPosition, hit.position);
    const float distance = Vector3Length(toLight);
    if (distance <= RayHitEpsilon || distance > light.radius) {
        return {};
    }

    const Vector3 lightDir = Vector3Scale(toLight, 1.0f / distance);
    const float lambert = std::max(Vector3DotProduct(hit.normal, lightDir), 0.0f);
    if (lambert <= 0.0f) {
        return {};
    }

    if (light.castsShadow && IsOccluded(
                map,
                hit.position,
                GeometricNormalForHit(hit),
                lightPosition,
                surfaceRef,
                surfaceIndex,
                hit.triangleIndex,
                bvh,
                triangles,
                alphaOccluders,
                alphaMaskCache,
                softShadowSample,
                stats)) {
        return {};
    }

    const float t = std::clamp(1.0f - distance / light.radius, 0.0f, 1.0f);
    const float attenuation = t * t;
    const float scale = light.intensity * attenuation * lambert;
    return MakeDirectLightEvaluation(
            Vector3Scale(light.linearColor, scale), lightDir);
}

float SmoothStep(float edge0, float edge1, float value)
{
    if (std::fabs(edge1 - edge0) <= BakeEpsilon) {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return std::isfinite(t) ? SmoothStep01(t) : t;
}

float ConeCosine(float degrees)
{
    return std::cos(std::clamp(degrees, 0.0f, 179.0f) * (Pi / 180.0f));
}

Vector3 NormalizeOrFallback(Vector3 value, Vector3 fallback)
{
    return NormalizeVector3OrFallback(value, fallback, 0.00000001f);
}

DirectLightEvaluation EvaluateDirectLightSample(
        const SectorTopologyMap& map,
        const LightmapWorldSpotLight& light,
        Vector3 lightPosition,
        const RasterHit& hit,
        const SectorGeneratedSurfaceRef& surfaceRef,
        int surfaceIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        bool softShadowSample,
        BakeRayStats& stats)
{
    const Vector3 toLight = Vector3Subtract(lightPosition, hit.position);
    const float distance = Vector3Length(toLight);
    if (distance <= RayHitEpsilon || distance > light.range) {
        return {};
    }

    const Vector3 lightDir = Vector3Scale(toLight, 1.0f / distance);
    const float lambert = std::max(Vector3DotProduct(hit.normal, lightDir), 0.0f);
    if (lambert <= 0.0f) {
        return {};
    }

    const Vector3 spotDirection = NormalizeOrFallback(
            Vector3Subtract(light.target, light.position),
            Vector3{0.0f, -1.0f, 0.0f});
    const Vector3 fragmentDirectionFromLight = Vector3Scale(lightDir, -1.0f);
    const float coneDot = Vector3DotProduct(spotDirection, fragmentDirectionFromLight);
    const float innerDegrees = std::clamp(light.innerConeDegrees, 0.0f, 179.0f);
    const float outerDegrees = std::max(innerDegrees, std::clamp(light.outerConeDegrees, 0.0f, 179.0f));
    const float innerConeCos = ConeCosine(innerDegrees);
    const float outerConeCos = ConeCosine(outerDegrees);
    const float coneAttenuation = SmoothStep(outerConeCos, innerConeCos, coneDot);
    if (coneAttenuation <= 0.0f) {
        return {};
    }

    if (light.castsShadow && IsOccluded(
                map,
                hit.position,
                GeometricNormalForHit(hit),
                lightPosition,
                surfaceRef,
                surfaceIndex,
                hit.triangleIndex,
                bvh,
                triangles,
                alphaOccluders,
                alphaMaskCache,
                softShadowSample,
                stats)) {
        return {};
    }

    const float t = std::clamp(1.0f - distance / light.range, 0.0f, 1.0f);
    const float attenuation = t * t;
    const float scale = light.intensity * attenuation * lambert * coneAttenuation;
    return MakeDirectLightEvaluation(
            Vector3Scale(light.linearColor, scale), lightDir);
}

DirectLightEvaluation EvaluateDirectLightSample(
        const SectorTopologyMap& map,
        const LightmapWorldRectLight& light,
        Vector3 samplePosition,
        const RasterHit& hit,
        const SectorGeneratedSurfaceRef& surfaceRef,
        int surfaceIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        BakeRayStats& stats)
{
    const Vector3 fromEmitter = Vector3Subtract(hit.position, samplePosition);
    const float distance = Vector3Length(fromEmitter);
    if (distance <= RayHitEpsilon || distance > light.range) return {};
    const Vector3 emitterToHit = Vector3Scale(fromEmitter, 1.0f / distance);
    const float emitterCosine = Vector3DotProduct(light.basis.forward, emitterToHit);
    if (emitterCosine <= 0.0f) return {};
    const float startAttenuation = SectorRectLightStartFeatherAttenuation(
            Vector3DotProduct(fromEmitter, light.basis.forward),
            light.startFeather);
    if (startAttenuation <= 0.0f) return {};
    const Vector3 directionToLight = Vector3Scale(emitterToHit, -1.0f);
    const float lambert = std::max(Vector3DotProduct(hit.normal, directionToLight), 0.0f);
    if (lambert <= 0.0f) return {};
    if (light.castsShadow && IsOccluded(
                map, hit.position, GeometricNormalForHit(hit), samplePosition,
                surfaceRef, surfaceIndex, hit.triangleIndex, bvh, triangles,
                alphaOccluders, alphaMaskCache, true, stats)) {
        return {};
    }
    const float t = std::clamp(1.0f - distance / light.range, 0.0f, 1.0f);
    const float scale = light.intensity * t * t * lambert * emitterCosine
            * startAttenuation;
    return MakeDirectLightEvaluation(
            Vector3Scale(light.linearColor, scale), directionToLight);
}

DirectLightEvaluation EvaluateDirectLight(
        const SectorTopologyMap& map,
        const LightmapWorldPointLight& light,
        const RasterHit& hit,
        const SectorGeneratedSurfaceRef& surfaceRef,
        int surfaceIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        int softShadowSampleCount,
        BakeRayStats& stats)
{
    if (light.radius <= 0.0f || light.intensity <= 0.0f) {
        return {};
    }

    const float sourceRadius = std::min(std::clamp(light.sourceRadius, 0.0f, 8.0f), light.radius * 0.5f);
    if (sourceRadius <= BakeEpsilon) {
        return EvaluateDirectLightSample(
                map,
                light,
                light.position,
                hit,
                surfaceRef,
                surfaceIndex,
                bvh,
                triangles,
                alphaOccluders,
                alphaMaskCache,
                false,
                stats);
    }

    DirectLightEvaluation direct;
    for (int i = 0; i < softShadowSampleCount; ++i) {
        const Vector3 sampleOffset = Vector3Scale(FibonacciSphereSample(i, softShadowSampleCount), sourceRadius);
        const Vector3 samplePosition = Vector3Add(light.position, sampleOffset);
        const DirectLightEvaluation sample = EvaluateDirectLightSample(
                        map,
                        light,
                        samplePosition,
                        hit,
                        surfaceRef,
                        surfaceIndex,
                        bvh,
                        triangles,
                        alphaOccluders,
                        alphaMaskCache,
                        true,
                        stats);
        direct.radiance = Vector3Add(direct.radiance, sample.radiance);
        direct.directionMoment = Vector3Add(
                direct.directionMoment, sample.directionMoment);
    }
    const float inverseSamples = 1.0f / static_cast<float>(softShadowSampleCount);
    direct.radiance = Vector3Scale(direct.radiance, inverseSamples);
    direct.directionMoment = Vector3Scale(direct.directionMoment, inverseSamples);
    return direct;
}

DirectLightEvaluation EvaluateDirectLight(
        const SectorTopologyMap& map,
        const LightmapWorldRectLight& light,
        const RasterHit& hit,
        const SectorGeneratedSurfaceRef& surfaceRef,
        int surfaceIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        int sampleCount,
        BakeRayStats& stats)
{
    if (light.range <= 0.0f || light.width <= 0.0f || light.height <= 0.0f
            || light.intensity <= 0.0f) return {};
    const int samples = std::max(1, sampleCount);
    auto radicalInverse = [](int index, int base) {
        float value = 0.0f;
        float factor = 1.0f / static_cast<float>(base);
        while (index > 0) {
            value += static_cast<float>(index % base) * factor;
            index /= base;
            factor /= static_cast<float>(base);
        }
        return value;
    };
    DirectLightEvaluation direct;
    for (int i = 0; i < samples; ++i) {
        const float u = radicalInverse(i + 1, 2) - 0.5f;
        const float v = radicalInverse(i + 1, 3) - 0.5f;
        const Vector3 samplePosition = Vector3Add(
                light.position,
                Vector3Add(
                        Vector3Scale(light.basis.right, u * light.width),
                        Vector3Scale(light.basis.up, v * light.height)));
        const DirectLightEvaluation sample = EvaluateDirectLightSample(
                map, light, samplePosition, hit, surfaceRef, surfaceIndex, bvh,
                triangles, alphaOccluders, alphaMaskCache, stats);
        direct.radiance = Vector3Add(direct.radiance, sample.radiance);
        direct.directionMoment = Vector3Add(
                direct.directionMoment, sample.directionMoment);
    }
    const float inverseSamples = 1.0f / static_cast<float>(samples);
    direct.radiance = Vector3Scale(direct.radiance, inverseSamples);
    direct.directionMoment = Vector3Scale(direct.directionMoment, inverseSamples);
    return direct;
}

DirectLightEvaluation EvaluateDirectLight(
        const SectorTopologyMap& map,
        const LightmapWorldSpotLight& light,
        const RasterHit& hit,
        const SectorGeneratedSurfaceRef& surfaceRef,
        int surfaceIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        int softShadowSampleCount,
        BakeRayStats& stats)
{
    if (light.range <= 0.0f || light.intensity <= 0.0f) {
        return {};
    }

    const float sourceRadius = std::min(std::clamp(light.sourceRadius, 0.0f, 8.0f), light.range * 0.5f);
    if (sourceRadius <= BakeEpsilon) {
        return EvaluateDirectLightSample(
                map,
                light,
                light.position,
                hit,
                surfaceRef,
                surfaceIndex,
                bvh,
                triangles,
                alphaOccluders,
                alphaMaskCache,
                false,
                stats);
    }

    DirectLightEvaluation direct;
    for (int i = 0; i < softShadowSampleCount; ++i) {
        const Vector3 sampleOffset = Vector3Scale(FibonacciSphereSample(i, softShadowSampleCount), sourceRadius);
        const Vector3 samplePosition = Vector3Add(light.position, sampleOffset);
        const DirectLightEvaluation sample = EvaluateDirectLightSample(
                        map,
                        light,
                        samplePosition,
                        hit,
                        surfaceRef,
                        surfaceIndex,
                        bvh,
                        triangles,
                        alphaOccluders,
                        alphaMaskCache,
                        true,
                        stats);
        direct.radiance = Vector3Add(direct.radiance, sample.radiance);
        direct.directionMoment = Vector3Add(
                direct.directionMoment, sample.directionMoment);
    }
    const float inverseSamples = 1.0f / static_cast<float>(softShadowSampleCount);
    direct.radiance = Vector3Scale(direct.radiance, inverseSamples);
    direct.directionMoment = Vector3Scale(direct.directionMoment, inverseSamples);
    return direct;
}

bool IsSkyOwnedLightmapSurface(
        const SectorTopologyMap& map,
        const SectorGeneratedSurfaceRef& surfaceRef)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, surfaceRef.topologySectorId);
    return sector != nullptr && sector->ceilingSky;
}

bool IsDirectionOccluded(
        const SectorTopologyMap& map,
        Vector3 position,
        Vector3 normal,
        Vector3 directionToLight,
        float maxDistance,
        const SectorGeneratedSurfaceRef& sourceSurfaceRef,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        BakeRayStats& stats)
{
    if (maxDistance <= RayHitEpsilon || bvh.nodes.empty()) {
        return false;
    }
    const Vector3 origin = Vector3Add(position, Vector3Scale(normal, RayOriginEpsilon));
    return RaycastBakeOcclusionAlphaAware(
            map,
            alphaMaskCache,
            bvh,
            triangles,
            alphaOccluders,
            Ray{origin, directionToLight},
            maxDistance,
            sourceSurfaceRef,
            sourceSurfaceIndex,
            sourceTriangleIndex,
            &stats.directHardShadow
    );
}

DirectLightEvaluation EvaluateDirectionalLight(
        const SectorTopologyMap& map,
        const LightmapWorldDirectionalLight& light,
        const RasterHit& hit,
        const SectorGeneratedSurfaceRef& surfaceRef,
        int surfaceIndex,
        float shadowMaxDistance,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        BakeRayStats& stats)
{
    if (!light.enabled || light.intensity <= 0.0f) {
        return {};
    }
    const float lambert = std::max(Vector3DotProduct(hit.normal, light.directionToLight), 0.0f);
    if (lambert <= 0.0f) {
        return {};
    }
    if (IsDirectionOccluded(
                map,
                hit.position,
                GeometricNormalForHit(hit),
                light.directionToLight,
                shadowMaxDistance,
                surfaceRef,
                surfaceIndex,
                hit.triangleIndex,
                bvh,
                triangles,
                alphaOccluders,
                alphaMaskCache,
                stats)) {
        return {};
    }

    const float scale = light.intensity * lambert;
    return MakeDirectLightEvaluation(
            Vector3Scale(light.linearColor, scale), light.directionToLight);
}

Vector3 SanitizeNonNegativeRadiance(Vector3 value)
{
    return Vector3{
            std::isfinite(value.x) ? std::max(value.x, 0.0f) : 0.0f,
            std::isfinite(value.y) ? std::max(value.y, 0.0f) : 0.0f,
            std::isfinite(value.z) ? std::max(value.z, 0.0f) : 0.0f};
}

Vector3 SectorAmbientBaseline(const SectorTopologyMap& map, int sectorId)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, sectorId);
    if (sector == nullptr || sector->ambientIntensity <= 0.0f) {
        return Vector3{};
    }

    const float scale = sector->ambientIntensity;
    return Vector3{
            (static_cast<float>(sector->ambientColor.r) / 255.0f) * scale,
            (static_cast<float>(sector->ambientColor.g) / 255.0f) * scale,
            (static_cast<float>(sector->ambientColor.b) / 255.0f) * scale};
}

BakedObjectLightingSample MakeObjectLightingSampleFromCube(const Vector3 (&ambientCube)[6], bool valid)
{
    BakedObjectLightingSample sample;
    sample.valid = valid;
    for (int face = 0; face < 6; ++face) {
        sample.ambientCube[face] = SanitizeNonNegativeRadiance(ambientCube[face]);
    }
    return sample;
}

BakedObjectLightingSample MakeNeutralObjectLightingSample()
{
    BakedObjectLightingSample sample;
    sample.valid = false;
    for (Vector3& face : sample.ambientCube) {
        face = Vector3{0.15f, 0.15f, 0.15f};
    }
    return sample;
}

BakedObjectLightingSample MakeSectorAmbientObjectLightingSample(const SectorTopologyMap& map, int sectorId)
{
    BakedObjectLightingSample sample;
    sample.valid = false;
    const Vector3 ambient = SanitizeNonNegativeRadiance(SectorAmbientBaseline(map, sectorId));
    for (Vector3& face : sample.ambientCube) {
        face = ambient;
    }
    return sample;
}

const SectorBakedObjectLightProbeSectorRange* FindProbeSectorRange(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        int sectorId,
        SectorBakedObjectLightProbeLayer layer)
{
    int begin = 0;
    int end = static_cast<int>(probes.sectorRanges.size());
    while (begin < end) {
        const int middle = begin + (end - begin) / 2;
        const SectorBakedObjectLightProbeSectorRange& range = probes.sectorRanges[static_cast<size_t>(middle)];
        const bool before = range.sectorId < sectorId
                || (range.sectorId == sectorId
                    && static_cast<unsigned int>(range.layer)
                            < static_cast<unsigned int>(layer));
        const bool after = range.sectorId > sectorId
                || (range.sectorId == sectorId
                    && static_cast<unsigned int>(range.layer)
                            > static_cast<unsigned int>(layer));
        if (before) {
            begin = middle + 1;
        } else if (after) {
            end = middle;
        } else {
            return &range;
        }
    }
    return nullptr;
}

struct ObjectProbeSelection {
    static constexpr int kMaxSampleProbes = 4;
    static constexpr float kExactProbeDistanceSquared = 0.000001f;
    int selectedIndices[kMaxSampleProbes] = {-1, -1, -1, -1};
    float selectedDistanceSquared[kMaxSampleProbes] = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
    int selectedCount = 0;
};

void StreamObjectLightProbeRange(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        Vector3 worldPosition,
        int begin,
        int count,
        ObjectProbeSelection& selection)
{
    for (int offset = 0; offset < count; ++offset) {
        const int probeIndex = begin + offset;
        if (probeIndex < 0 || probeIndex >= static_cast<int>(probes.probes.size())) {
            continue;
        }

        const Vector3 probePosition =
                probes.probes[static_cast<size_t>(probeIndex)].position;
        const float deltaX = worldPosition.x - probePosition.x;
        const float deltaZ = worldPosition.z - probePosition.z;
        const float distanceSquared = deltaX * deltaX + deltaZ * deltaZ;
        if (!std::isfinite(distanceSquared)) {
            continue;
        }

        int insertAt = selection.selectedCount < ObjectProbeSelection::kMaxSampleProbes
                ? selection.selectedCount
                : -1;
        for (int selected = 0; selected < selection.selectedCount; ++selected) {
            if (distanceSquared < selection.selectedDistanceSquared[selected]) {
                insertAt = selected;
                break;
            }
        }
        if (insertAt < 0 || insertAt >= ObjectProbeSelection::kMaxSampleProbes) {
            continue;
        }

        const int shiftEnd = std::min(selection.selectedCount, ObjectProbeSelection::kMaxSampleProbes - 1);
        for (int shift = shiftEnd; shift > insertAt; --shift) {
            selection.selectedIndices[shift] = selection.selectedIndices[shift - 1];
            selection.selectedDistanceSquared[shift] = selection.selectedDistanceSquared[shift - 1];
        }
        selection.selectedIndices[insertAt] = probeIndex;
        selection.selectedDistanceSquared[insertAt] = distanceSquared;
        selection.selectedCount =
                std::min(selection.selectedCount + 1, ObjectProbeSelection::kMaxSampleProbes);
    }
}

bool HasSelectedObjectLightProbes(const ObjectProbeSelection& selection)
{
    return selection.selectedCount > 0;
}

struct SelectedObjectProbeLayerSample {
    BakedObjectLightingSample lighting;
    float heightWorld = 0.0f;
};

SelectedObjectProbeLayerSample SampleSelectedObjectLightProbes(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const ObjectProbeSelection& selection)
{
    if (selection.selectedCount == 0) {
        return SelectedObjectProbeLayerSample{
                MakeNeutralObjectLightingSample(), 0.0f};
    }

    if (selection.selectedDistanceSquared[0] <= ObjectProbeSelection::kExactProbeDistanceSquared) {
        const SectorBakedObjectLightProbe& probe =
                probes.probes[static_cast<size_t>(selection.selectedIndices[0])];
        return SelectedObjectProbeLayerSample{
                MakeObjectLightingSampleFromCube(probe.ambientCube, true),
                probe.position.y};
    }

    BakedObjectLightingSample sample;
    sample.valid = true;
    float totalWeight = 0.0f;
    float weightedHeight = 0.0f;
    for (int selected = 0; selected < selection.selectedCount; ++selected) {
        const float distance = std::sqrt(selection.selectedDistanceSquared[selected]);
        const float weight = 1.0f / std::max(distance, 0.001f);
        totalWeight += weight;
        const SectorBakedObjectLightProbe& probe =
                probes.probes[static_cast<size_t>(selection.selectedIndices[selected])];
        weightedHeight += probe.position.y * weight;
        for (int face = 0; face < 6; ++face) {
            sample.ambientCube[face] = Vector3Add(
                    sample.ambientCube[face],
                    Vector3Scale(probe.ambientCube[face], weight));
        }
    }

    if (totalWeight <= 0.0f || !std::isfinite(totalWeight)) {
        return SelectedObjectProbeLayerSample{
                MakeNeutralObjectLightingSample(), 0.0f};
    }

    for (Vector3& face : sample.ambientCube) {
        face = SanitizeNonNegativeRadiance(Vector3Scale(face, 1.0f / totalWeight));
    }
    return SelectedObjectProbeLayerSample{
            sample,
            weightedHeight / totalWeight};
}

float DistanceSquaredPointToSegment2(Vector2 point, Vector2 a, Vector2 b)
{
    const Vector2 segment = Vector2Subtract(b, a);
    const float lengthSquared = Vector2LengthSqr(segment);
    if (lengthSquared <= 0.0f || !std::isfinite(lengthSquared)) {
        return Vector2DistanceSqr(point, a);
    }

    const float t = std::clamp(Vector2DotProduct(Vector2Subtract(point, a), segment) / lengthSquared, 0.0f, 1.0f);
    const Vector2 closest = Vector2Add(a, Vector2Scale(segment, t));
    return Vector2DistanceSqr(point, closest);
}

bool AddAdjacentObjectProbeSectorId(
        int sectorId,
        int preferredSectorId,
        int (&adjacentSectorIds)[kObjectProbeMaxAdjacentBlendSectors],
        int& adjacentSectorCount)
{
    if (sectorId == preferredSectorId) {
        return false;
    }
    for (int index = 0; index < adjacentSectorCount; ++index) {
        if (adjacentSectorIds[index] == sectorId) {
            return false;
        }
    }
    if (adjacentSectorCount >= kObjectProbeMaxAdjacentBlendSectors) {
        return false;
    }

    adjacentSectorIds[adjacentSectorCount] = sectorId;
    ++adjacentSectorCount;
    return true;
}

const SectorBakedObjectLightProbePortalRange* FindObjectProbePortalRange(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        int sectorId)
{
    const auto it = std::lower_bound(
            probes.portalRanges.begin(),
            probes.portalRanges.end(),
            sectorId,
            [](const SectorBakedObjectLightProbePortalRange& range, int id) {
                return range.sectorId < id;
            });
    return it != probes.portalRanges.end() && it->sectorId == sectorId
            ? &*it
            : nullptr;
}

void CollectAdjacentObjectProbeSectorIdsNearPortals(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        Vector3 worldPosition,
        int preferredSectorId,
        int (&adjacentSectorIds)[kObjectProbeMaxAdjacentBlendSectors],
        int& adjacentSectorCount)
{
    adjacentSectorCount = 0;
    const Vector2 samplePosition = Vector2{worldPosition.x, worldPosition.z};
    const float blendDistanceSquared =
            kObjectProbeAdjacentPortalBlendDistanceWorld * kObjectProbeAdjacentPortalBlendDistanceWorld;

    const SectorBakedObjectLightProbePortalRange* range =
            FindObjectProbePortalRange(probes, preferredSectorId);
    if (range == nullptr || range->count <= 0) {
        return;
    }

    const int end = std::min(
            range->begin + range->count,
            static_cast<int>(probes.portals.size()));
    for (int portalIndex = std::max(0, range->begin);
            portalIndex < end;
            ++portalIndex) {
        if (adjacentSectorCount >= kObjectProbeMaxAdjacentBlendSectors) {
            break;
        }
        const SectorBakedObjectLightProbePortal& portal =
                probes.portals[static_cast<size_t>(portalIndex)];
        const float distanceSquared = DistanceSquaredPointToSegment2(
                samplePosition,
                portal.startWorld,
                portal.endWorld);
        if (!std::isfinite(distanceSquared) || distanceSquared > blendDistanceSquared) {
            continue;
        }

        AddAdjacentObjectProbeSectorId(
                portal.adjacentSectorId,
                preferredSectorId,
                adjacentSectorIds,
                adjacentSectorCount);
    }
}

Vector3 EvaluateProbePointLight(
        const SectorTopologyMap& map,
        const LightmapWorldPointLight& light,
        Vector3 probePosition,
        Vector3 faceDirection,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        int softShadowSampleCount,
        BakeRayStats& stats)
{
    RasterHit hit;
    hit.hit = true;
    hit.position = probePosition;
    hit.normal = faceDirection;
    hit.triangleIndex = -1;
    return EvaluateDirectLight(
            map,
            light,
            hit,
            SectorGeneratedSurfaceRef{},
            -1,
            bvh,
            triangles,
            alphaOccluders,
            alphaMaskCache,
            softShadowSampleCount,
            stats).radiance;
}

Vector3 EvaluateProbeSpotLight(
        const SectorTopologyMap& map,
        const LightmapWorldSpotLight& light,
        Vector3 probePosition,
        Vector3 faceDirection,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        int softShadowSampleCount,
        BakeRayStats& stats)
{
    RasterHit hit;
    hit.hit = true;
    hit.position = probePosition;
    hit.normal = faceDirection;
    hit.triangleIndex = -1;
    return EvaluateDirectLight(
            map,
            light,
            hit,
            SectorGeneratedSurfaceRef{},
            -1,
            bvh,
            triangles,
            alphaOccluders,
            alphaMaskCache,
            softShadowSampleCount,
            stats).radiance;
}

Vector3 EvaluateProbeRectLight(
        const SectorTopologyMap& map,
        const LightmapWorldRectLight& light,
        Vector3 probePosition,
        Vector3 faceDirection,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        int sampleCount,
        BakeRayStats& stats)
{
    RasterHit hit;
    hit.hit = true;
    hit.position = probePosition;
    hit.normal = faceDirection;
    hit.triangleIndex = -1;
    return EvaluateDirectLight(map, light, hit, SectorGeneratedSurfaceRef{}, -1,
            bvh, triangles, alphaOccluders, alphaMaskCache, sampleCount, stats).radiance;
}

Vector3 EvaluateProbeDirectionalLight(
        const SectorTopologyMap& map,
        const LightmapWorldDirectionalLight& light,
        Vector3 probePosition,
        Vector3 faceDirection,
        float shadowMaxDistance,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        BakeRayStats& stats)
{
    RasterHit hit;
    hit.hit = true;
    hit.position = probePosition;
    hit.normal = faceDirection;
    hit.triangleIndex = -1;
    return EvaluateDirectionalLight(
            map,
            light,
            hit,
            SectorGeneratedSurfaceRef{},
            -1,
            shadowMaxDistance,
            bvh,
            triangles,
            alphaOccluders,
            alphaMaskCache,
            stats).radiance;
}

void BakeProbeAmbientCube(
        const SectorTopologyMap& map,
        const std::vector<LightmapWorldPointLight>& worldLights,
        const std::vector<LightmapWorldSpotLight>& worldSpotLights,
        const std::vector<LightmapWorldRectLight>& worldRectLights,
        const LightmapWorldDirectionalLight& directionalLight,
        float directionalShadowMaxDistance,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        SectorLightmapAlphaMaskCache& alphaMaskCache,
        int softShadowSampleCount,
        BakeRayStats& stats,
        SectorBakedObjectLightProbe& probe)
{
    constexpr Vector3 faceDirections[6] = {
            Vector3{1.0f, 0.0f, 0.0f},
            Vector3{-1.0f, 0.0f, 0.0f},
            Vector3{0.0f, 1.0f, 0.0f},
            Vector3{0.0f, -1.0f, 0.0f},
            Vector3{0.0f, 0.0f, 1.0f},
            Vector3{0.0f, 0.0f, -1.0f}};
    const Vector3 ambient = SectorAmbientBaseline(map, probe.sectorId);

    for (int face = 0; face < 6; ++face) {
        const Vector3 faceDirection = faceDirections[face];
        Vector3 rgb = ambient;
        for (const LightmapWorldPointLight& light : worldLights) {
            rgb = Vector3Add(
                    rgb,
                    EvaluateProbePointLight(
                            map,
                            light,
                            probe.position,
                            faceDirection,
                            bvh,
                            triangles,
                            alphaOccluders,
                            alphaMaskCache,
                            softShadowSampleCount,
                            stats));
        }
        for (const LightmapWorldSpotLight& light : worldSpotLights) {
            rgb = Vector3Add(
                    rgb,
                    EvaluateProbeSpotLight(
                            map,
                            light,
                            probe.position,
                            faceDirection,
                            bvh,
                            triangles,
                            alphaOccluders,
                            alphaMaskCache,
                            softShadowSampleCount,
                            stats));
        }
        for (const LightmapWorldRectLight& light : worldRectLights) {
            rgb = Vector3Add(rgb, EvaluateProbeRectLight(
                    map, light, probe.position, faceDirection, bvh, triangles,
                    alphaOccluders, alphaMaskCache, softShadowSampleCount, stats));
        }
        rgb = Vector3Add(
                rgb,
                EvaluateProbeDirectionalLight(
                        map,
                        directionalLight,
                        probe.position,
                        faceDirection,
                        directionalShadowMaxDistance,
                        bvh,
                        triangles,
                        alphaOccluders,
                        alphaMaskCache,
                        stats));
        probe.ambientCube[face] = SanitizeNonNegativeRadiance(rgb);
    }
}

LightmapWorldPointLight MakeWorldSpaceLight(const SectorTopologyStaticPointLight& authoringLight)
{
    LightmapWorldPointLight light;
    light.position = SectorAuthoringToWorldPosition(authoringLight.position);
    light.linearColor = SectorLightmapAuthoredSrgbColorToLinear(authoringLight.color);
    light.intensity = authoringLight.intensity;
    light.radius = SectorAuthoringToWorldDistance(authoringLight.radius);
    light.sourceRadius = SectorAuthoringToWorldDistance(authoringLight.sourceRadius);
    light.castsShadow = authoringLight.castsShadow;
    return light;
}

LightmapWorldSpotLight MakeWorldSpaceLight(const SectorTopologyStaticSpotLight& authoringLight)
{
    LightmapWorldSpotLight light;
    light.position = SectorAuthoringToWorldPosition(authoringLight.position);
    light.target = SectorAuthoringToWorldPosition(authoringLight.target);
    light.linearColor = SectorLightmapAuthoredSrgbColorToLinear(authoringLight.color);
    light.intensity = authoringLight.intensity;
    light.range = SectorAuthoringToWorldDistance(authoringLight.range);
    light.sourceRadius = SectorAuthoringToWorldDistance(authoringLight.sourceRadius);
    light.innerConeDegrees = authoringLight.innerConeDegrees;
    light.outerConeDegrees = authoringLight.outerConeDegrees;
    light.castsShadow = authoringLight.castsShadow;
    return light;
}

LightmapWorldRectLight MakeWorldSpaceLight(const SectorTopologyStaticRectLight& authoringLight)
{
    LightmapWorldRectLight light;
    light.position = SectorAuthoringToWorldPosition(authoringLight.position);
    const Vector3 target = SectorAuthoringToWorldPosition(authoringLight.target);
    light.basis = BuildSectorRectLightBasis(light.position, target, authoringLight.rollDegrees);
    light.linearColor = SectorLightmapAuthoredSrgbColorToLinear(authoringLight.color);
    light.intensity = authoringLight.intensity;
    light.range = SectorAuthoringToWorldDistance(authoringLight.range);
    light.width = SectorAuthoringToWorldDistance(authoringLight.width);
    light.height = SectorAuthoringToWorldDistance(authoringLight.height);
    light.startFeather = std::clamp(
            SectorAuthoringToWorldDistance(authoringLight.startFeather),
            0.0f,
            light.range);
    light.castsShadow = authoringLight.castsShadow;
    return light;
}

LightmapWorldDirectionalLight MakeWorldSpaceDirectionalLight(
        const SectorTopologyDirectionalLightSettings& authoringLight)
{
    const SectorTopologyDirectionalLightSettings normalized =
            NormalizeSectorTopologyDirectionalLightSettings(authoringLight);
    LightmapWorldDirectionalLight light;
    light.enabled = normalized.enabled;
    light.directionToLight = normalized.directionToLight;
    light.linearColor = SectorLightmapAuthoredSrgbColorToLinear(normalized.color);
    light.intensity = normalized.intensity;
    return light;
}

float BakeAmbientOcclusion(
        const RasterHit& hit,
        const SectorGeneratedSurfaceRef& surfaceRef,
        int surfaceIndex,
        float radius,
        float strength,
        int sampleCount,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        BakeRayStats& stats)
{
    if (strength <= 0.0f || radius <= BakeEpsilon) {
        return 1.0f;
    }

    const Vector3 origin = Vector3Add(hit.position, Vector3Scale(hit.normal, RayOriginEpsilon));
    float occlusion = 0.0f;
    for (int i = 0; i < sampleCount; ++i) {
        const Vector3 direction = CosineHemisphereSample(hit.normal, i, sampleCount);
        const RayHit rayHit = TraceRay(origin, direction, radius, surfaceRef, surfaceIndex, hit.triangleIndex, &stats.ambientOcclusion, bvh, triangles);
        if (!rayHit.hit) {
            continue;
        }

        occlusion += 1.0f - std::clamp(rayHit.distance / radius, 0.0f, 1.0f);
    }

    const float averageOcclusion = occlusion / static_cast<float>(sampleCount);
    return std::clamp(1.0f - strength * averageOcclusion, 0.0f, 1.0f);
}

std::vector<BakeTriangle> BuildBakeTriangles(
        const SectorGeneratedGeometry& geometry,
        const SectorLightmapLayout& layout,
        const SectorStaticModelLightmapData* staticModels = nullptr)
{
    std::vector<BakeTriangle> triangles;
    for (size_t surfaceIndex = 0; surfaceIndex < geometry.surfaces.size(); ++surfaceIndex) {
        const SectorGeneratedSurface& surface = geometry.surfaces[surfaceIndex];
        if (!CastsLightmapOcclusion(surface)) {
            continue;
        }
        const SectorLightmapChart* chart = surfaceIndex < layout.charts.size()
                        && layout.charts[surfaceIndex].surfaceIndex >= 0
                ? &layout.charts[surfaceIndex]
                : nullptr;
        for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
            Vector3 normal = surface.normal;
            if (Vector3LengthSqr(normal) <= BakeEpsilon) {
                const Vector3 edge1 = Vector3Subtract(surface.vertices[i + 1].position, surface.vertices[i + 0].position);
                const Vector3 edge2 = Vector3Subtract(surface.vertices[i + 2].position, surface.vertices[i + 0].position);
                normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
            }
            const Vector2 uv0 = chart != nullptr && i + 0 < chart->vertexUvs.size() ? chart->vertexUvs[i + 0] : Vector2{};
            const Vector2 uv1 = chart != nullptr && i + 1 < chart->vertexUvs.size() ? chart->vertexUvs[i + 1] : Vector2{};
            const Vector2 uv2 = chart != nullptr && i + 2 < chart->vertexUvs.size() ? chart->vertexUvs[i + 2] : Vector2{};
            triangles.push_back(BakeTriangle{
                    surface.vertices[i + 0].position,
                    surface.vertices[i + 1].position,
                    surface.vertices[i + 2].position,
                    Vector3Normalize(normal),
                    uv0,
                    uv1,
                    uv2,
                    chart != nullptr ? chart->atlasIndex : -1,
                    surface.ref,
                    static_cast<int>(surfaceIndex),
                    static_cast<int>(i / 3)
            });
        }
    }
    if (staticModels == nullptr) {
        return triangles;
    }

    int staticSurfaceIndex = static_cast<int>(geometry.surfaces.size());
    for (const SectorStaticModelLightmapObject& object : staticModels->objects) {
        if (object.modelIndex < 0
                || object.modelIndex >= static_cast<int>(staticModels->models.size())) {
            continue;
        }
        const SectorStaticModelLightmapModel& model =
                staticModels->models[static_cast<size_t>(object.modelIndex)];
        for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
            if (meshIndex >= object.meshPlacements.size()) {
                ++staticSurfaceIndex;
                continue;
            }
            const SectorStaticModelLightmapMesh& mesh = model.meshes[meshIndex];
            const SectorStaticModelLightmapMeshPlacement& placement =
                    object.meshPlacements[meshIndex];
            const SectorGeneratedSurfaceRef surfaceRef{
                    SectorGeneratedSurfaceKind::Middle,
                    object.containingSectorId,
                    -1,
                    -1,
                    SectorTopologySideKind::Front};
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const uint32_t ia = mesh.indices[i];
                const uint32_t ib = mesh.indices[i + 1];
                const uint32_t ic = mesh.indices[i + 2];
                if (ia >= mesh.importedPositions.size()
                        || ib >= mesh.importedPositions.size()
                        || ic >= mesh.importedPositions.size()
                        || ia >= mesh.importedNormals.size()
                        || ib >= mesh.importedNormals.size()
                        || ic >= mesh.importedNormals.size()
                        || ia >= mesh.localLightmapUvs.size()
                        || ib >= mesh.localLightmapUvs.size()
                        || ic >= mesh.localLightmapUvs.size()) {
                    continue;
                }
                const Vector3 position0 =
                        TransformStaticModelPosition(mesh.importedPositions[ia], object);
                const Vector3 position1 =
                        TransformStaticModelPosition(mesh.importedPositions[ib], object);
                const Vector3 position2 =
                        TransformStaticModelPosition(mesh.importedPositions[ic], object);
                Vector3 normal = Vector3Normalize(Vector3Add(
                        Vector3Add(
                                TransformStaticModelNormal(mesh.importedNormals[ia], object),
                                TransformStaticModelNormal(mesh.importedNormals[ib], object)),
                        TransformStaticModelNormal(mesh.importedNormals[ic], object)));
                if (Vector3LengthSqr(normal) <= BakeEpsilon) {
                    normal = Vector3Normalize(Vector3CrossProduct(
                            Vector3Subtract(position1, position0),
                            Vector3Subtract(position2, position0)));
                }
                const auto atlasUv = [&placement](Vector2 uv) {
                    return Vector2{
                            placement.atlasBias.x + uv.x * placement.atlasScale.x,
                            placement.atlasBias.y + uv.y * placement.atlasScale.y};
                };
                triangles.push_back(BakeTriangle{
                        position0,
                        position1,
                        position2,
                        normal,
                        atlasUv(mesh.localLightmapUvs[ia]),
                        atlasUv(mesh.localLightmapUvs[ib]),
                        atlasUv(mesh.localLightmapUvs[ic]),
                        placement.atlasIndex,
                        surfaceRef,
                        staticSurfaceIndex,
                        static_cast<int>(i / 3),
                        object.castsShadow});
            }
            ++staticSurfaceIndex;
        }
    }
    return triangles;
}

std::vector<SectorLightmapAlphaOccluderTriangle> BuildAlphaTestOccluderTriangles(
        const SectorGeneratedGeometry& geometry)
{
    std::vector<SectorLightmapAlphaOccluderTriangle> triangles;
    for (size_t surfaceIndex = 0; surfaceIndex < geometry.surfaces.size(); ++surfaceIndex) {
        const SectorGeneratedSurface& surface = geometry.surfaces[surfaceIndex];
        if (!CastsAlphaTestLightmapOcclusion(surface)) {
            continue;
        }
        for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
            Vector3 normal = surface.normal;
            if (Vector3LengthSqr(normal) <= BakeEpsilon) {
                const Vector3 edge1 = Vector3Subtract(surface.vertices[i + 1].position, surface.vertices[i + 0].position);
                const Vector3 edge2 = Vector3Subtract(surface.vertices[i + 2].position, surface.vertices[i + 0].position);
                normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
            }
            triangles.push_back(SectorLightmapAlphaOccluderTriangle{
                    surface.vertices[i + 0].position,
                    surface.vertices[i + 1].position,
                    surface.vertices[i + 2].position,
                    Vector3Normalize(normal),
                    surface.vertices[i + 0].uv,
                    surface.vertices[i + 1].uv,
                    surface.vertices[i + 2].uv,
                    surface.materialId,
                    surface.alphaCutoff,
                    surface.ref,
                    static_cast<int>(surfaceIndex),
                    static_cast<int>(i / 3)
            });
        }
    }
    return triangles;
}

Vector2 ChartLocalToAtlasUv(
        const SectorLightmapChart& chart,
        const SectorGeneratedSurface& surface,
        Vector2 local,
        int atlasWidth,
        int atlasHeight)
{
    const float localU = surface.chartWidth <= BakeEpsilon ? 0.0f : std::clamp(local.x / surface.chartWidth, 0.0f, 1.0f);
    const float localV = surface.chartHeight <= BakeEpsilon ? 0.0f : std::clamp(local.y / surface.chartHeight, 0.0f, 1.0f);
    const float minX = static_cast<float>(chart.usableX) + 0.5f;
    const float minY = static_cast<float>(chart.usableY) + 0.5f;
    const float maxX = static_cast<float>(chart.usableX + chart.usableWidth) - 0.5f;
    const float maxY = static_cast<float>(chart.usableY + chart.usableHeight) - 0.5f;
    return Vector2{
            (minX + (maxX - minX) * localU) / static_cast<float>(atlasWidth),
            (minY + (maxY - minY) * localV) / static_cast<float>(atlasHeight)
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
        std::vector<Vector4>& pixels,
        std::vector<unsigned char>& valid,
        int atlasWidth,
        int atlasHeight)
{
    const size_t atlasOffset = static_cast<size_t>(chart.atlasIndex)
            * static_cast<size_t>(atlasWidth)
            * static_cast<size_t>(atlasHeight);
    for (int y = chart.y; y < chart.y + chart.height; ++y) {
        for (int x = chart.x; x < chart.x + chart.width; ++x) {
            const size_t index = atlasOffset + static_cast<size_t>(y * atlasWidth + x);
            if (valid[index] != 0) {
                continue;
            }

            int bestDistance2 = std::numeric_limits<int>::max();
            Vector4 best{0.0f, 0.0f, 0.0f, 1.0f};
            for (int sy = chart.usableY; sy < chart.usableY + chart.usableHeight; ++sy) {
                for (int sx = chart.usableX; sx < chart.usableX + chart.usableWidth; ++sx) {
                    const size_t sourceIndex = atlasOffset + static_cast<size_t>(sy * atlasWidth + sx);
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
        int atlasWidth,
        int atlasHeight)
{
    const size_t atlasOffset = static_cast<size_t>(chart.atlasIndex)
            * static_cast<size_t>(atlasWidth)
            * static_cast<size_t>(atlasHeight);
    for (int y = chart.y; y < chart.y + chart.height; ++y) {
        for (int x = chart.x; x < chart.x + chart.width; ++x) {
            const size_t index = atlasOffset + static_cast<size_t>(y * atlasWidth + x);
            if (valid[index] != 0) {
                continue;
            }

            int bestDistance2 = std::numeric_limits<int>::max();
            Vector3 best{};
            for (int sy = chart.usableY; sy < chart.usableY + chart.usableHeight; ++sy) {
                for (int sx = chart.usableX; sx < chart.usableX + chart.usableWidth; ++sx) {
                    const size_t sourceIndex = atlasOffset + static_cast<size_t>(sy * atlasWidth + sx);
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
        int atlasIndex,
        Vector2 lightmapUv)
{
    if (atlasIndex < 0) {
        return Vector3{};
    }
    const size_t atlasOffset = static_cast<size_t>(atlasIndex)
            * static_cast<size_t>(atlasWidth)
            * static_cast<size_t>(atlasHeight);
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
        const size_t index = atlasOffset + static_cast<size_t>(sample.y * atlasWidth + sample.x);
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
                const size_t index = atlasOffset + static_cast<size_t>(y * atlasWidth + x);
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

void RemoveFileIfExists(const std::string& path)
{
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
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

bool BuildSectorLightmapLayoutFromGeometry(
        const SectorGeneratedGeometry& geometry,
        float texelsPerWorldUnit,
        SectorLightmapLayout& outLayout,
        std::string& outError)
{
    outLayout = SectorLightmapLayout{};
    outLayout.texelsPerWorldUnit = texelsPerWorldUnit;
    outLayout.charts.resize(geometry.surfaces.size());
    int shelfX = 0;
    int shelfY = 0;
    int shelfHeight = 0;
    int atlasIndex = 0;

    for (size_t surfaceIndex = 0; surfaceIndex < geometry.surfaces.size(); ++surfaceIndex) {
        const SectorGeneratedSurface& surface = geometry.surfaces[surfaceIndex];
        if (!surface.receivesLightmap) {
            continue;
        }
        const int usableWidth = std::max(2, static_cast<int>(std::ceil(surface.chartWidth * texelsPerWorldUnit)));
        const int usableHeight = std::max(2, static_cast<int>(std::ceil(surface.chartHeight * texelsPerWorldUnit)));
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
            ++atlasIndex;
            shelfX = 0;
            shelfY = 0;
            shelfHeight = 0;
        }

        SectorLightmapChart chart;
        chart.surfaceIndex = static_cast<int>(surfaceIndex);
        chart.atlasIndex = atlasIndex;
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
            chart.vertexUvs.push_back(ChartLocalToAtlasUv(
                    chart,
                    surface,
                    vertex.chartUv,
                    outLayout.atlasWidth,
                    outLayout.atlasHeight));
        }
        outLayout.charts[surfaceIndex] = std::move(chart);

        shelfX += chartWidth;
        shelfHeight = std::max(shelfHeight, chartHeight);
    }

    outLayout.atlasCount = atlasIndex + 1;

    return true;
}

SectorStaticModelLightmapPackCursor StaticModelPackCursorAfterTopology(
        const SectorLightmapLayout& layout)
{
    SectorStaticModelLightmapPackCursor cursor;
    cursor.atlasIndex = std::max(0, layout.atlasCount - 1);
    bool found = false;
    for (const SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex < 0 || chart.width <= 0 || chart.height <= 0) {
            continue;
        }
        if (chart.atlasIndex != cursor.atlasIndex) {
            continue;
        }
        if (!found || chart.y > cursor.shelfY) {
            cursor.shelfY = chart.y;
            cursor.shelfX = chart.x + chart.width;
            cursor.shelfHeight = chart.height;
            found = true;
        } else if (chart.y == cursor.shelfY) {
            cursor.shelfX = std::max(cursor.shelfX, chart.x + chart.width);
            cursor.shelfHeight = std::max(cursor.shelfHeight, chart.height);
        }
    }
    return cursor;
}

SectorLightmapChart PlacementAsChart(
        const SectorStaticModelLightmapMeshPlacement& placement)
{
    SectorLightmapChart chart;
    chart.atlasIndex = placement.atlasIndex;
    chart.x = placement.x;
    chart.y = placement.y;
    chart.width = placement.width;
    chart.height = placement.height;
    chart.usableX = placement.usableX;
    chart.usableY = placement.usableY;
    chart.usableWidth = placement.usableWidth;
    chart.usableHeight = placement.usableHeight;
    return chart;
}

bool BuildLightmapGeneratedGeometryForBake(
        const SectorTopologyMap& map,
        SectorGeneratedGeometry& outGeometry,
        std::string& outError)
{
    if (!BuildSectorGeneratedGeometry(map, outGeometry, &outError)) {
        outError = outError.empty()
                ? "Bake failed: no generated topology sector surfaces"
                : "Bake failed: " + outError;
        return false;
    }
    return true;
}

void FnvAppendColor(uint64_t& hash, Color color)
{
    FnvAppendInt(hash, static_cast<int>(color.r));
    FnvAppendInt(hash, static_cast<int>(color.g));
    FnvAppendInt(hash, static_cast<int>(color.b));
    FnvAppendInt(hash, static_cast<int>(color.a));
}

void FnvAppendVector2(uint64_t& hash, Vector2 value)
{
    FnvAppendFloat(hash, value.x);
    FnvAppendFloat(hash, value.y);
}

void FnvAppendVector3(uint64_t& hash, Vector3 value)
{
    FnvAppendFloat(hash, value.x);
    FnvAppendFloat(hash, value.y);
    FnvAppendFloat(hash, value.z);
}

void FnvAppendTopologyUv(uint64_t& hash, const SectorTopologyUvSettings& uv)
{
    FnvAppendVector2(hash, uv.scale);
    FnvAppendVector2(hash, uv.offset);
}

void FnvAppendTopologyWallPart(uint64_t& hash, const SectorTopologyWallPartSettings& part)
{
    FnvAppendString(hash, part.materialId);
    FnvAppendTopologyUv(hash, part.uv);
}

void FnvAppendLightmapBakeConstantsAndSettings(
        uint64_t& hash,
        const SectorLightmapBakeSettings& settings)
{
    const SectorLightmapBakeQualityParameters quality =
            ResolveSectorLightmapBakeQuality(settings.qualityPreset);
    FnvAppendString(hash, "slice3-linear-hdr-baked-illumination");
    FnvAppendString(hash, "authored-static-light-swatches-srgb-decode-once");
    FnvAppendString(hash, kSectorLightmapArtifactFormat);
    FnvAppendInt(hash, kSectorLightmapArtifactVersion);
    FnvAppendString(hash, kSectorBakedObjectLightProbeSidecarFormat);
    FnvAppendInt(hash, kSectorBakedObjectLightProbeSidecarVersion);
    FnvAppendString(hash, kSectorStaticModelLightmapSidecarFormat);
    FnvAppendInt(hash, kSectorStaticModelLightmapSidecarVersion);
    FnvAppendString(hash, "indirect-direct-buffer-neutral-albedo-no-surface-color");
    FnvAppendInt(hash, kSectorLightmapBakeVersion);
    FnvAppendInt(hash, SectorLightmapAtlasWidth);
    FnvAppendInt(hash, SectorLightmapAtlasHeight);
    FnvAppendInt(hash, SectorLightmapGutterTexels);
    FnvAppendFloat(hash, quality.texelsPerWorldUnit);
    FnvAppendFloat(hash, kSectorWorldUnitsPerAuthoringUnit);
    FnvAppendInt(hash, quality.directSoftShadowSampleCount);
    FnvAppendInt(hash, quality.ambientOcclusionSampleCount);
    FnvAppendInt(hash, quality.indirectBounceSampleCount);
    FnvAppendFloat(hash, kNeutralBounceAlbedo);
    FnvAppendFloat(hash, std::clamp(SectorAuthoringToWorldDistance(settings.ambientOcclusionRadius), 0.05f, 16.0f));
    FnvAppendFloat(hash, std::clamp(settings.ambientOcclusionStrength, 0.0f, 1.0f));
    FnvAppendFloat(hash, std::clamp(SectorAuthoringToWorldDistance(settings.indirectBounceRadius), 0.05f, 16.0f));
    FnvAppendFloat(hash, std::clamp(settings.indirectBounceStrength, 0.0f, 1.0f));
    FnvAppendFloat(hash, std::clamp(settings.objectProbeSpacingWorld, 0.25f, 128.0f));
    FnvAppendFloat(hash, std::clamp(settings.objectProbeLowerHeightWorld, 0.0f, 16.0f));
    FnvAppendFloat(hash, std::clamp(settings.objectProbeUpperHeightWorld, 0.0f, 16.0f));
}

void FnvAppendDirectionalLightSettings(
        uint64_t& hash,
        const SectorTopologyDirectionalLightSettings& settings)
{
    const SectorTopologyDirectionalLightSettings normalized =
            NormalizeSectorTopologyDirectionalLightSettings(settings);
    FnvAppendInt(hash, normalized.enabled ? 1 : 0);
    FnvAppendVector3(hash, normalized.directionToLight);
    FnvAppendInt(hash, static_cast<int>(normalized.color.r));
    FnvAppendInt(hash, static_cast<int>(normalized.color.g));
    FnvAppendInt(hash, static_cast<int>(normalized.color.b));
    FnvAppendFloat(hash, normalized.intensity);
}

template<typename T>
std::vector<const T*> SortedLightmapHashRecords(const std::vector<T>& values)
{
    std::vector<const T*> sorted;
    sorted.reserve(values.size());
    for (const T& value : values) {
        sorted.push_back(&value);
    }
    std::sort(sorted.begin(), sorted.end(), [](const T* left, const T* right) {
        return left->id < right->id;
    });
    return sorted;
}

void AddReferencedLightmapTexture(std::unordered_set<std::string>& materialIds, const std::string& materialId)
{
    if (!materialId.empty()) {
        materialIds.insert(materialId);
    }
}

std::vector<std::string> SortedReferencedLightmapTextureIds(const SectorTopologyMap& map)
{
    std::unordered_set<std::string> referenced;
    for (const SectorTopologySideDef& sideDef : map.sideDefs) {
        AddReferencedLightmapTexture(referenced, sideDef.wall.materialId);
        AddReferencedLightmapTexture(referenced, sideDef.lower.materialId);
        AddReferencedLightmapTexture(referenced, sideDef.upper.materialId);
        AddReferencedLightmapTexture(referenced, sideDef.middle.materialId);
    }
    for (const SectorTopologySector& sector : map.sectors) {
        AddReferencedLightmapTexture(referenced, sector.floorMaterialId);
        AddReferencedLightmapTexture(referenced, sector.ceilingMaterialId);
        AddReferencedLightmapTexture(referenced, sector.defaultWall.materialId);
        AddReferencedLightmapTexture(referenced, sector.defaultLower.materialId);
        AddReferencedLightmapTexture(referenced, sector.defaultUpper.materialId);
    }
    for (const SectorCompiledStructuralPrimitive& primitive
            : map.compiledStructuralPrimitives) {
        AddReferencedLightmapTexture(
                referenced,
                primitive.authored.materials.defaultSurface.materialId);
        for (const SectorStructuralMaterialOverride& override
                : primitive.authored.materials.overrides) {
            if (override.enabled) {
                AddReferencedLightmapTexture(referenced, override.settings.materialId);
            }
        }
    }

    std::vector<std::string> ids;
    ids.reserve(referenced.size());
    for (const std::string& materialId : referenced) {
        if (map.resolvedMaterialsById.find(materialId) != map.resolvedMaterialsById.end()) {
            ids.push_back(materialId);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

void BakeObjectProbeAmbientCubesInScene(
        const SectorTopologyMap& map,
        const SectorLightmapBvh& bvh,
        const std::vector<BakeTriangle>& triangles,
        const std::vector<SectorLightmapAlphaOccluderTriangle>& alphaOccluders,
        std::vector<SectorBakedObjectLightProbe>& probes)
{
    const int softShadowSampleCount = ResolveSectorLightmapBakeQuality(
            map.lightmapSettings.qualityPreset).directSoftShadowSampleCount;
    std::vector<LightmapWorldPointLight> worldLights;
    worldLights.reserve(map.staticLights.size());
    for (const SectorTopologyStaticPointLight& light : map.staticLights) {
        worldLights.push_back(MakeWorldSpaceLight(light));
    }
    std::vector<LightmapWorldSpotLight> worldSpotLights;
    worldSpotLights.reserve(map.staticSpotLights.size());
    for (const SectorTopologyStaticSpotLight& light : map.staticSpotLights) {
        worldSpotLights.push_back(MakeWorldSpaceLight(light));
    }
    std::vector<LightmapWorldRectLight> worldRectLights;
    worldRectLights.reserve(map.staticRectLights.size());
    for (const SectorTopologyStaticRectLight& light : map.staticRectLights) {
        worldRectLights.push_back(MakeWorldSpaceLight(light));
    }
    const LightmapWorldDirectionalLight directionalLight =
            MakeWorldSpaceDirectionalLight(map.directionalLight);
    const float directionalShadowMaxDistance = BvhSceneDiagonalWithMargin(bvh);
    SectorLightmapAlphaMaskCache alphaMaskCache;
    BakeRayStats stats;
    for (SectorBakedObjectLightProbe& probe : probes) {
        BakeProbeAmbientCube(
                map,
                worldLights,
                worldSpotLights,
                worldRectLights,
                directionalLight,
                directionalShadowMaxDistance,
                bvh,
                triangles,
                alphaOccluders,
                alphaMaskCache,
                softShadowSampleCount,
                stats,
                probe);
    }
}

} // namespace

Vector3 SectorLightmapAuthoredSrgbColorToLinear(Color color)
{
    const auto decode = [](unsigned char channel) {
        const float srgb = static_cast<float>(channel) / 255.0f;
        return srgb <= 0.04045f
                ? srgb / 12.92f
                : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
    };
    return Vector3{decode(color.r), decode(color.g), decode(color.b)};
}

float SectorLightmapBinary16ToFloat(uint16_t bits)
{
    const uint32_t sign = static_cast<uint32_t>(bits & 0x8000u) << 16u;
    int32_t exponent = static_cast<int32_t>((bits >> 10u) & 0x1fu);
    uint32_t mantissa = bits & 0x03ffu;
    uint32_t valueBits = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            valueBits = sign;
        } else {
            exponent = 1;
            while ((mantissa & 0x0400u) == 0) {
                mantissa <<= 1u;
                --exponent;
            }
            mantissa &= 0x03ffu;
            valueBits = sign
                    | (static_cast<uint32_t>(exponent + 112) << 23u)
                    | (mantissa << 13u);
        }
    } else if (exponent == 0x1f) {
        valueBits = sign | 0x7f800000u | (mantissa << 13u);
    } else {
        valueBits = sign
                | (static_cast<uint32_t>(exponent + 112) << 23u)
                | (mantissa << 13u);
    }
    return FloatFromLittleEndianBits(valueBits);
}

uint16_t SectorLightmapFloatToBinary16(float value)
{
    const uint32_t bits = FloatToLittleEndianBits(value);
    const uint16_t sign = static_cast<uint16_t>((bits >> 16u) & 0x8000u);
    const uint32_t exponent = (bits >> 23u) & 0xffu;
    const uint32_t mantissa = bits & 0x007fffffu;
    if (exponent == 0xffu) {
        return static_cast<uint16_t>(sign | 0x7c00u
                | (mantissa == 0 ? 0u : std::max(1u, mantissa >> 13u)));
    }

    const int32_t halfExponent = static_cast<int32_t>(exponent) - 127 + 15;
    if (halfExponent >= 31) {
        return static_cast<uint16_t>(sign | 0x7c00u);
    }
    if (halfExponent <= 0) {
        if (halfExponent < -10) {
            return sign;
        }
        const uint32_t normalizedMantissa = mantissa | 0x00800000u;
        const unsigned int shift = static_cast<unsigned int>(14 - halfExponent);
        uint32_t halfMantissa = normalizedMantissa >> shift;
        const uint32_t remainderMask = (1u << shift) - 1u;
        const uint32_t remainder = normalizedMantissa & remainderMask;
        const uint32_t halfway = 1u << (shift - 1u);
        if (remainder > halfway
                || (remainder == halfway && (halfMantissa & 1u) != 0)) {
            ++halfMantissa;
        }
        return static_cast<uint16_t>(sign | halfMantissa);
    }

    uint32_t halfMantissa = mantissa >> 13u;
    const uint32_t remainder = mantissa & 0x1fffu;
    uint32_t encodedExponent = static_cast<uint32_t>(halfExponent);
    if (remainder > 0x1000u
            || (remainder == 0x1000u && (halfMantissa & 1u) != 0)) {
        ++halfMantissa;
        if (halfMantissa == 0x0400u) {
            halfMantissa = 0;
            ++encodedExponent;
            if (encodedExponent >= 31u) {
                return static_cast<uint16_t>(sign | 0x7c00u);
            }
        }
    }
    return static_cast<uint16_t>(sign | (encodedExponent << 10u) | halfMantissa);
}

bool WriteSectorLightmapArtifact(
        const std::string& path,
        int width,
        int height,
        const Vector4* linearRgba,
        const Vector4* directionalRgba,
        size_t texelCount,
        const std::string& sourceHash,
        SectorIlluminationStatistics& outPreEncodeStatistics,
        SectorIlluminationStatistics& outStoredStatistics,
        std::string& outError)
{
    outError.clear();
    outPreEncodeStatistics = {};
    outStoredStatistics = {};
    if (path.empty() || width <= 0 || height <= 0 || linearRgba == nullptr
            || directionalRgba == nullptr || sourceHash.empty()) {
        outError = "HDR lightmap write failed: invalid arguments";
        return false;
    }
    const uint64_t expectedTexels = static_cast<uint64_t>(width)
            * static_cast<uint64_t>(height);
    if (expectedTexels != texelCount
            || expectedTexels > std::numeric_limits<uint64_t>::max() / 12u
            || sourceHash.size() > std::numeric_limits<uint32_t>::max()) {
        outError = "HDR lightmap write failed: invalid dimensions or source hash";
        return false;
    }

    std::vector<uint8_t> payload;
    payload.reserve(texelCount * 12u);
    for (size_t index = 0; index < texelCount; ++index) {
        const Vector4 value = linearRgba[index];
        const Vector3 rgb{value.x, value.y, value.z};
        if (!IsFiniteVector3(rgb) || rgb.x < 0.0f || rgb.y < 0.0f
                || rgb.z < 0.0f || rgb.x > 65504.0f || rgb.y > 65504.0f
                || rgb.z > 65504.0f || !std::isfinite(value.w)
                || value.w < 0.0f || value.w > 1.0f) {
            outError = "HDR lightmap write failed: invalid radiance or AO";
            return false;
        }
        AccumulateStatistics(outPreEncodeStatistics, rgb, value.w);
        const uint16_t channels[4] = {
                SectorLightmapFloatToBinary16(value.x),
                SectorLightmapFloatToBinary16(value.y),
                SectorLightmapFloatToBinary16(value.z),
                SectorLightmapFloatToBinary16(value.w)};
        for (const uint16_t channel : channels) {
            payload.push_back(static_cast<uint8_t>(channel & 0xffu));
            payload.push_back(static_cast<uint8_t>((channel >> 8u) & 0xffu));
        }
    }
    for (size_t index = 0; index < texelCount; ++index) {
        const Vector4 value = directionalRgba[index];
        if (!std::isfinite(value.x) || !std::isfinite(value.y)
                || !std::isfinite(value.z) || !std::isfinite(value.w)
                || value.x < 0.0f || value.x > 1.0f
                || value.y < 0.0f || value.y > 1.0f
                || value.z < 0.0f || value.z > 1.0f
                || value.w < 0.0f || value.w > 1.0f) {
            outError = "HDR lightmap write failed: invalid directional sample";
            return false;
        }
        payload.push_back(static_cast<uint8_t>(std::lround(value.x * 255.0f)));
        payload.push_back(static_cast<uint8_t>(std::lround(value.y * 255.0f)));
        payload.push_back(static_cast<uint8_t>(std::lround(value.z * 255.0f)));
        payload.push_back(static_cast<uint8_t>(std::lround(value.w * 255.0f)));
    }

    const std::filesystem::path outputPath(path);
    std::error_code ec;
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            outError = "HDR lightmap write failed: could not create output directory";
            return false;
        }
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    const uint32_t headerBytes = kLightmapArtifactFixedHeaderBytes
            + static_cast<uint32_t>(sourceHash.size());
    output.write(kLightmapArtifactMagic, sizeof(kLightmapArtifactMagic));
    if (!output.good()
            || !WriteU32LE(output, kSectorLightmapArtifactVersion)
            || !WriteU32LE(output, headerBytes)
            || !WriteU32LE(output, static_cast<uint32_t>(width))
            || !WriteU32LE(output, static_cast<uint32_t>(height))
            || !WriteU32LE(output, kLightmapArtifactChannels)
            || !WriteU32LE(output, kLightmapArtifactEncodingMixedRgba16Rgba8)
            || !WriteU32LE(output, kLightmapArtifactSemanticsHdrRgbAoDominantDirection)
            || !WriteU32LE(output, static_cast<uint32_t>(sourceHash.size()))
            || !WriteU64LE(output, static_cast<uint64_t>(payload.size()))
            || !WriteU64LE(output, Fnv1aBytes(payload))) {
        outError = "HDR lightmap write failed: header write failed";
        return false;
    }
    output.write(sourceHash.data(), static_cast<std::streamsize>(sourceHash.size()));
    output.write(reinterpret_cast<const char*>(payload.data()),
            static_cast<std::streamsize>(payload.size()));
    output.close();
    if (!output.good()) {
        outError = "HDR lightmap write failed: payload write failed";
        return false;
    }

    SectorLightmapArtifactData reopened;
    SectorLightmapMetadata expected;
    expected.width = width;
    expected.height = height;
    expected.version = kSectorLightmapArtifactVersion;
    expected.format = kSectorLightmapArtifactFormat;
    expected.sourceHash = sourceHash;
    if (!ReadSectorLightmapArtifact(path, &expected, reopened, outError)) {
        outError = "HDR lightmap write verification failed: " + outError;
        return false;
    }
    outStoredStatistics = reopened.storedStatistics;
    return true;
}

bool ReadSectorLightmapArtifact(
        const std::string& path,
        const SectorLightmapMetadata* expectedMetadata,
        SectorLightmapArtifactData& outData,
        std::string& outError)
{
    outData = {};
    outError.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        outError = "HDR lightmap read failed: missing artifact";
        return false;
    }
    char magic[4] = {};
    input.read(magic, sizeof(magic));
    uint32_t version = 0;
    uint32_t headerBytes = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    uint32_t encoding = 0;
    uint32_t semantics = 0;
    uint32_t sourceHashBytes = 0;
    uint64_t payloadBytes = 0;
    uint64_t payloadChecksum = 0;
    if (input.gcount() != static_cast<std::streamsize>(sizeof(magic))
            || !std::equal(std::begin(magic), std::end(magic),
                    std::begin(kLightmapArtifactMagic))
            || !ReadU32LE(input, version)
            || !ReadU32LE(input, headerBytes)
            || !ReadU32LE(input, width)
            || !ReadU32LE(input, height)
            || !ReadU32LE(input, channels)
            || !ReadU32LE(input, encoding)
            || !ReadU32LE(input, semantics)
            || !ReadU32LE(input, sourceHashBytes)
            || !ReadU64LE(input, payloadBytes)
            || !ReadU64LE(input, payloadChecksum)) {
        outError = "HDR lightmap read failed: invalid or truncated header";
        return false;
    }
    if (version != static_cast<uint32_t>(kSectorLightmapArtifactVersion)
            || headerBytes != kLightmapArtifactFixedHeaderBytes + sourceHashBytes
            || width == 0 || height == 0 || channels != kLightmapArtifactChannels
            || encoding != kLightmapArtifactEncodingMixedRgba16Rgba8
            || semantics != kLightmapArtifactSemanticsHdrRgbAoDominantDirection
            || sourceHashBytes == 0 || sourceHashBytes > 1024u) {
        outError = "HDR lightmap read failed: unsupported or invalid header";
        return false;
    }
    const uint64_t texelCount = static_cast<uint64_t>(width) * height;
    if (texelCount > std::numeric_limits<uint64_t>::max() / 12u
            || payloadBytes != texelCount * 12u
            || payloadBytes > std::numeric_limits<size_t>::max()) {
        outError = "HDR lightmap read failed: invalid payload size";
        return false;
    }
    std::string sourceHash(sourceHashBytes, '\0');
    input.read(sourceHash.data(), static_cast<std::streamsize>(sourceHash.size()));
    std::vector<uint8_t> payload(static_cast<size_t>(payloadBytes));
    input.read(reinterpret_cast<char*>(payload.data()),
            static_cast<std::streamsize>(payload.size()));
    if (input.gcount() != static_cast<std::streamsize>(payload.size())
            || input.peek() != std::char_traits<char>::eof()
            || Fnv1aBytes(payload) != payloadChecksum) {
        outError = "HDR lightmap read failed: truncated, trailing, or corrupt payload";
        return false;
    }
    if (expectedMetadata != nullptr
            && ((expectedMetadata->version != 0
                        && expectedMetadata->version != static_cast<int>(version))
                || (!expectedMetadata->format.empty()
                        && expectedMetadata->format != kSectorLightmapArtifactFormat)
                || (expectedMetadata->width > 0
                        && expectedMetadata->width != static_cast<int>(width))
                || (expectedMetadata->height > 0
                        && expectedMetadata->height != static_cast<int>(height))
                || (!expectedMetadata->sourceHash.empty()
                        && expectedMetadata->sourceHash != sourceHash))) {
        outError = "HDR lightmap read failed: metadata mismatch";
        return false;
    }

    outData.width = static_cast<int>(width);
    outData.height = static_cast<int>(height);
    outData.sourceHash = std::move(sourceHash);
    outData.rgba16.resize(static_cast<size_t>(texelCount) * 4u);
    for (size_t channelIndex = 0; channelIndex < outData.rgba16.size(); ++channelIndex) {
        const size_t byteIndex = channelIndex * 2u;
        const uint16_t bits = static_cast<uint16_t>(payload[byteIndex])
                | static_cast<uint16_t>(
                        static_cast<uint16_t>(payload[byteIndex + 1u]) << 8u);
        outData.rgba16[channelIndex] = bits;
    }
    const size_t directionalByteOffset = static_cast<size_t>(texelCount) * 8u;
    outData.directionalRgba8.assign(
            payload.begin() + static_cast<std::ptrdiff_t>(directionalByteOffset),
            payload.end());
    for (size_t texelIndex = 0; texelIndex < static_cast<size_t>(texelCount); ++texelIndex) {
        const size_t base = texelIndex * 4u;
        const Vector3 rgb{
                SectorLightmapBinary16ToFloat(outData.rgba16[base]),
                SectorLightmapBinary16ToFloat(outData.rgba16[base + 1u]),
                SectorLightmapBinary16ToFloat(outData.rgba16[base + 2u])};
        const float ao = SectorLightmapBinary16ToFloat(outData.rgba16[base + 3u]);
        if (!IsFiniteVector3(rgb) || rgb.x < 0.0f || rgb.y < 0.0f
                || rgb.z < 0.0f || !std::isfinite(ao) || ao < 0.0f
                || ao > 1.0f) {
            outData = {};
            outError = "HDR lightmap read failed: invalid radiance or AO";
            return false;
        }
        AccumulateStatistics(outData.storedStatistics, rgb, ao);
    }
    return true;
}

std::string SectorLightmapAlphaMaskCache::CacheKey(const SectorTopologyMap& map, const std::string& materialId)
{
    const auto it = map.resolvedMaterialsById.find(materialId);
    if (it == map.resolvedMaterialsById.end()) {
        return materialId + "\n\n";
    }

    return materialId + "\n" + it->second.id + "\n" + it->second.path;
}

const SectorLightmapAlphaMaskCache::AlphaMask& SectorLightmapAlphaMaskCache::LoadOrGet(
        const SectorTopologyMap& map,
        const std::string& materialId)
{
    const std::string key = CacheKey(map, materialId);
    const auto found = masksByKey.find(key);
    if (found != masksByKey.end()) {
        return found->second;
    }

    ++loadAttemptsByKey[key];

    AlphaMask mask;
    const auto textureIt = map.resolvedMaterialsById.find(materialId);
    if (textureIt == map.resolvedMaterialsById.end()) {
        TraceLog(LOG_WARNING, "Sector lightmap alpha cache missing texture id '%s'", materialId.c_str());
        return masksByKey.emplace(key, std::move(mask)).first->second;
    }

    const std::string resolvedPath = ResolveSectorAssetPath(textureIt->second.path);
    Image image = LoadImage(resolvedPath.c_str());
    if (image.data == nullptr || image.width <= 0 || image.height <= 0) {
        TraceLog(LOG_WARNING,
                 "Sector lightmap alpha cache could not load texture '%s' from '%s'",
                 materialId.c_str(),
                 resolvedPath.c_str());
        if (image.data != nullptr) {
            UnloadImage(image);
        }
        return masksByKey.emplace(key, std::move(mask)).first->second;
    }

    Color* colors = LoadImageColors(image);
    if (colors == nullptr) {
        TraceLog(LOG_WARNING,
                 "Sector lightmap alpha cache could not read pixels for texture '%s' from '%s'",
                 materialId.c_str(),
                 resolvedPath.c_str());
        UnloadImage(image);
        return masksByKey.emplace(key, std::move(mask)).first->second;
    }

    mask.valid = true;
    mask.width = image.width;
    mask.height = image.height;
    mask.alpha.resize(static_cast<size_t>(image.width * image.height), 255);
    for (int i = 0; i < image.width * image.height; ++i) {
        mask.alpha[static_cast<size_t>(i)] = colors[i].a;
    }

    UnloadImageColors(colors);
    UnloadImage(image);
    return masksByKey.emplace(key, std::move(mask)).first->second;
}

SectorLightmapAlphaSample SectorLightmapAlphaMaskCache::Sample(
        const SectorTopologyMap& map,
        const std::string& materialId,
        Vector2 uv,
        float alphaCutoff)
{
    const AlphaMask& mask = LoadOrGet(map, materialId);
    SectorLightmapAlphaSample sample;
    sample.valid = mask.valid;
    sample.width = mask.width;
    sample.height = mask.height;
    if (!mask.valid || mask.width <= 0 || mask.height <= 0 || mask.alpha.empty()) {
        sample.opaque = true;
        sample.alpha = 255;
        return sample;
    }

    auto wrapUnit = [](float value) {
        if (!std::isfinite(value)) {
            return 0.0f;
        }
        return value - std::floor(value);
    };

    const float wrappedU = wrapUnit(uv.x);
    const float wrappedV = wrapUnit(uv.y);
    const int x = std::clamp(static_cast<int>(std::floor(wrappedU * static_cast<float>(mask.width))), 0, mask.width - 1);
    const int y = std::clamp(static_cast<int>(std::floor(wrappedV * static_cast<float>(mask.height))), 0, mask.height - 1);
    const size_t index = static_cast<size_t>(y * mask.width + x);
    sample.alpha = index < mask.alpha.size() ? mask.alpha[index] : 255;
    sample.opaque = (static_cast<float>(sample.alpha) / 255.0f) >= alphaCutoff;
    return sample;
}

size_t SectorLightmapAlphaMaskCache::CachedTextureCount() const
{
    return masksByKey.size();
}

int SectorLightmapAlphaMaskCache::LoadAttemptCount(const SectorTopologyMap& map, const std::string& materialId) const
{
    const std::string key = CacheKey(map, materialId);
    const auto found = loadAttemptsByKey.find(key);
    return found == loadAttemptsByKey.end() ? 0 : found->second;
}

std::vector<SectorLightmapAlphaOccluderTriangle> CollectSectorLightmapAlphaOccluders(
        const SectorGeneratedGeometry& geometry)
{
    return BuildAlphaTestOccluderTriangles(geometry);
}

bool BuildSectorBakedObjectLightProbePlacements(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbePlacementSettings& settings,
        std::vector<SectorBakedObjectLightProbe>& outProbes,
        std::vector<SectorBakedObjectLightProbePlacementDiagnostic>* outDiagnostics,
        std::string& outError)
{
    outError.clear();
    outProbes.clear();
    if (outDiagnostics != nullptr) {
        outDiagnostics->clear();
    }

    if (!std::isfinite(settings.probeSpacingWorld) || settings.probeSpacingWorld <= 0.0f) {
        outError = "Object probe placement failed: probe spacing must be positive";
        return false;
    }
    if (!std::isfinite(settings.lowerHeightWorld)
            || !std::isfinite(settings.upperHeightWorld)
            || settings.lowerHeightWorld < 0.0f
            || settings.upperHeightWorld < settings.lowerHeightWorld) {
        outError = "Object probe placement failed: layer heights must be finite, non-negative, and ordered";
        return false;
    }

    const double spacingCoord = static_cast<double>(SectorWorldToAuthoringDistance(settings.probeSpacingWorld))
            * static_cast<double>(SectorCoordSubdivisions);
    if (!(spacingCoord > 0.0) || !std::isfinite(spacingCoord)) {
        outError = "Object probe placement failed: invalid probe spacing";
        return false;
    }
    const double boundaryClearanceCoord =
            static_cast<double>(SectorWorldToAuthoringDistance(
                    kObjectProbeSurfaceClearanceWorld))
            * static_cast<double>(SectorCoordSubdivisions);

    const SectorTopologyIndexes indexes = BuildSectorTopologyIndexes(map);
    for (const SectorTopologySector& sector : map.sectors) {
        const ResolvedObjectProbeLayerHeights layerHeights =
                ResolveObjectProbeLayerHeights(sector, settings, outDiagnostics);
        SectorTopologyLoopSet loops;
        std::vector<SectorTopologyValidationIssue> loopIssues;
        if (!ExtractSectorTopologyLoops(map, indexes, sector.id, loops, &loopIssues)) {
            outError = "Object probe placement failed: could not extract loops for sector "
                    + std::to_string(sector.id);
            return false;
        }

        std::vector<SectorTopologyCoordPoint> outer;
        if (!AppendLoopPolygon(map, loops.outer, outer, outError)) {
            return false;
        }

        std::vector<std::vector<SectorTopologyCoordPoint>> holes;
        holes.reserve(loops.holes.size());
        for (const SectorTopologyLoop& holeLoop : loops.holes) {
            std::vector<SectorTopologyCoordPoint> hole;
            if (!AppendLoopPolygon(map, holeLoop, hole, outError)) {
                return false;
            }
            holes.push_back(std::move(hole));
        }

        SectorCoord minX = outer.front().x;
        SectorCoord minY = outer.front().y;
        SectorCoord maxX = outer.front().x;
        SectorCoord maxY = outer.front().y;
        for (const SectorTopologyCoordPoint point : outer) {
            minX = std::min(minX, point.x);
            minY = std::min(minY, point.y);
            maxX = std::max(maxX, point.x);
            maxY = std::max(maxY, point.y);
        }

        const size_t beforeCount = outProbes.size();
        for (double y = static_cast<double>(minY) + spacingCoord * 0.5;
                y < static_cast<double>(maxY);
                y += spacingCoord) {
            for (double x = static_cast<double>(minX) + spacingCoord * 0.5;
                    x < static_cast<double>(maxX);
                    x += spacingCoord) {
                if (x < static_cast<double>(std::numeric_limits<SectorCoord>::min())
                        || x > static_cast<double>(std::numeric_limits<SectorCoord>::max())
                        || y < static_cast<double>(std::numeric_limits<SectorCoord>::min())
                        || y > static_cast<double>(std::numeric_limits<SectorCoord>::max())) {
                    continue;
                }

                const SectorTopologyCoordPoint candidate{
                        static_cast<SectorCoord>(std::llround(x)),
                        static_cast<SectorCoord>(std::llround(y))};
                if (!IsValidProbePolygonPoint(
                            outer, holes, candidate, boundaryClearanceCoord)) {
                    continue;
                }

                AppendObjectProbeLayers(
                        outProbes,
                        sector.id,
                        candidate,
                        layerHeights);
            }
        }

        if (outProbes.size() == beforeCount) {
            SectorTopologyCoordPoint representative;
            if (!FindRepresentativeProbePoint(
                        outer,
                        holes,
                        minX,
                        minY,
                        maxX,
                        maxY,
                        boundaryClearanceCoord,
                        representative)) {
                if (outDiagnostics != nullptr) {
                    outDiagnostics->push_back(SectorBakedObjectLightProbePlacementDiagnostic{
                            sector.id,
                            "Object probe placement skipped the sector because no candidate satisfied the required surface clearance"});
                }
                continue;
            }

            AppendObjectProbeLayers(
                    outProbes,
                    sector.id,
                    representative,
                    layerHeights);
            if (outDiagnostics != nullptr) {
                outDiagnostics->push_back(SectorBakedObjectLightProbePlacementDiagnostic{
                        sector.id,
                        "Object probe placement used a representative fallback point because no grid point survived"});
            }
        }
    }

    return true;
}

bool IsSectorLightmapStaticRayOccludedForTests(
        const SectorTopologyMap& map,
        const SectorGeneratedGeometry& geometry,
        const SectorLightmapLayout& layout,
        Ray ray,
        float maxDistance)
{
    const std::vector<BakeTriangle> triangles = BuildBakeTriangles(geometry, layout);
    SectorLightmapBvh bvh;
    BakeBvhBuildStats bvhStats;
    std::string error;
    if (!BuildSectorLightmapBvh(triangles, bvh, bvhStats, error)) {
        return false;
    }

    SectorLightmapAlphaMaskCache alphaMaskCache;
    SectorLightmapRaycastStats stats;
    const std::vector<SectorLightmapAlphaOccluderTriangle> alphaOccluders =
            CollectSectorLightmapAlphaOccluders(geometry);
    return RaycastBakeOcclusionAlphaAware(
            map,
            alphaMaskCache,
            bvh,
            triangles,
            alphaOccluders,
            ray,
            maxDistance,
            SectorGeneratedSurfaceRef{},
            -1,
            -1,
            &stats);
}

bool BakeSectorBakedObjectLightProbeAmbientCubes(
        const SectorTopologyMap& map,
        std::vector<SectorBakedObjectLightProbe>& probes,
        std::string& outError)
{
    outError.clear();
    for (const SectorBakedObjectLightProbe& probe : probes) {
        if (!IsFiniteVector3(probe.position)) {
            outError = "Object probe bake failed: non-finite probe position";
            return false;
        }
    }

    SectorGeneratedGeometry geometry;
    if (!BuildLightmapGeneratedGeometryForBake(map, geometry, outError)) {
        return false;
    }

    SectorLightmapLayout layout;
    const SectorLightmapBakeQualityParameters quality =
            ResolveSectorLightmapBakeQuality(map.lightmapSettings.qualityPreset);
    if (!BuildSectorLightmapLayoutFromGeometry(
                geometry,
                quality.texelsPerWorldUnit,
                layout,
                outError)) {
        return false;
    }

    const std::vector<BakeTriangle> triangles = BuildBakeTriangles(geometry, layout);
    SectorLightmapBvh bvh;
    BakeBvhBuildStats bvhStats;
    if (!BuildSectorLightmapBvh(triangles, bvh, bvhStats, outError)) {
        if (outError.empty()) {
            outError = "Object probe bake failed: could not build lightmap BVH";
        }
        return false;
    }

    const std::vector<SectorLightmapAlphaOccluderTriangle> alphaOccluders =
            CollectSectorLightmapAlphaOccluders(geometry);
    BakeObjectProbeAmbientCubesInScene(
            map,
            bvh,
            triangles,
            alphaOccluders,
            probes);

    return true;
}

bool IsSameLogicalSectorLightmapSurface(
        const SectorGeneratedSurfaceRef& a,
        const SectorGeneratedSurfaceRef& b)
{
    if (a.sourceKind != b.sourceKind) {
        return false;
    }
    if (a.sourceKind == SectorGeneratedSurfaceSourceKind::StructuralPrimitive) {
        if (a.structuralFace.primitiveId != b.structuralFace.primitiveId
                || a.structuralFace.role != b.structuralFace.role
                || a.structuralFace.roleIndex != b.structuralFace.roleIndex) {
            return false;
        }
        return a.structuralFace.role != SectorStructuralFaceRole::CylinderSide
                && a.structuralFace.role != SectorStructuralFaceRole::SphereSurface;
    }
    if (a.kind != b.kind) return false;

    switch (a.kind) {
        case SectorGeneratedSurfaceKind::Floor:
        case SectorGeneratedSurfaceKind::Ceiling:
            return a.topologySectorId == b.topologySectorId;
        case SectorGeneratedSurfaceKind::Wall:
        case SectorGeneratedSurfaceKind::LowerWall:
        case SectorGeneratedSurfaceKind::UpperWall:
            return a.topologySectorId == b.topologySectorId
                    && a.topologyLineDefId == b.topologyLineDefId
                    && a.topologySideDefId == b.topologySideDefId
                    && a.topologySide == b.topologySide;
        case SectorGeneratedSurfaceKind::Middle:
            return false;
    }

    return false;
}

bool WriteSectorBakedObjectLightProbeSidecar(
        const std::string& path,
        const std::vector<SectorBakedObjectLightProbe>& probes,
        float probeSpacingWorld,
        float probeLowerHeightWorld,
        float probeUpperHeightWorld,
        const std::string& sourceHash,
        std::string& outError)
{
    outError.clear();
    if (path.empty() || sourceHash.empty()
            || sourceHash.size() > std::numeric_limits<uint32_t>::max()) {
        outError = "Object probe sidecar write failed: missing output path";
        return false;
    }
    if (!std::isfinite(probeSpacingWorld)
            || !std::isfinite(probeLowerHeightWorld)
            || !std::isfinite(probeUpperHeightWorld)
            || probeLowerHeightWorld < 0.0f
            || probeUpperHeightWorld < probeLowerHeightWorld) {
        outError = "Object probe sidecar write failed: non-finite probe settings";
        return false;
    }
    if (probes.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
        outError = "Object probe sidecar write failed: too many probes";
        return false;
    }

    for (const SectorBakedObjectLightProbe& probe : probes) {
        if (probe.layer != SectorBakedObjectLightProbeLayer::Lower
                && probe.layer != SectorBakedObjectLightProbeLayer::Upper) {
            outError = "Object probe sidecar write failed: invalid probe layer";
            return false;
        }
        if (!IsFiniteVector3(probe.position)) {
            outError = "Object probe sidecar write failed: non-finite probe position";
            return false;
        }
        for (const Vector3& cubeFace : probe.ambientCube) {
            if (!IsFiniteVector3(cubeFace) || cubeFace.x < 0.0f
                    || cubeFace.y < 0.0f || cubeFace.z < 0.0f) {
                outError = "Object probe sidecar write failed: invalid ambient cube value";
                return false;
            }
        }
    }

    const std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            outError = "Object probe sidecar write failed: could not create output directory";
            return false;
        }
    }

    std::ostringstream payload(std::ios::binary);
    for (const SectorBakedObjectLightProbe& probe : probes) {
        if (!WriteI32LE(payload, static_cast<int32_t>(probe.sectorId))
                || !WriteU32LE(payload, static_cast<uint32_t>(probe.layer))
                || !WriteProbeVector(payload, probe.position)) {
            outError = "Object probe sidecar write failed: could not write probe record";
            return false;
        }
        for (const Vector3& cubeFace : probe.ambientCube) {
            if (!WriteProbeVector(payload, cubeFace)) {
                outError = "Object probe sidecar write failed: could not write probe ambient cube";
                return false;
            }
        }
    }
    const std::string payloadString = payload.str();
    const std::vector<uint8_t> payloadBytes(payloadString.begin(), payloadString.end());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    const uint32_t headerBytes = kObjectProbeFixedHeaderBytes
            + static_cast<uint32_t>(sourceHash.size());
    output.write(kObjectProbeSidecarMagic, sizeof(kObjectProbeSidecarMagic));
    if (!output.good()
            || !WriteU32LE(output, static_cast<uint32_t>(kSectorBakedObjectLightProbeSidecarVersion))
            || !WriteU32LE(output, headerBytes)
            || !WriteU32LE(output, static_cast<uint32_t>(probes.size()))
            || !WriteF32LE(output, probeSpacingWorld)
            || !WriteF32LE(output, probeLowerHeightWorld)
            || !WriteF32LE(output, probeUpperHeightWorld)
            || !WriteU32LE(output, static_cast<uint32_t>(sourceHash.size()))
            || !WriteU64LE(output, static_cast<uint64_t>(payloadBytes.size()))
            || !WriteU64LE(output, Fnv1aBytes(payloadBytes))) {
        outError = "Object probe sidecar write failed: could not write header";
        return false;
    }
    output.write(sourceHash.data(), static_cast<std::streamsize>(sourceHash.size()));
    output.write(payloadString.data(), static_cast<std::streamsize>(payloadString.size()));
    output.close();
    if (!output.good()) {
        outError = "Object probe sidecar write failed: output stream error";
        return false;
    }
    std::vector<SectorBakedObjectLightProbe> reopened;
    SectorBakedObjectLightProbeMetadata reopenedMetadata;
    SectorBakedObjectLightProbeMetadata expected;
    expected.version = kSectorBakedObjectLightProbeSidecarVersion;
    expected.sourceHash = sourceHash;
    expected.count = static_cast<int>(probes.size());
    expected.probeSpacingWorld = probeSpacingWorld;
    expected.probeLowerHeightWorld = probeLowerHeightWorld;
    expected.probeUpperHeightWorld = probeUpperHeightWorld;
    expected.format = kSectorBakedObjectLightProbeSidecarFormat;
    return ReadSectorBakedObjectLightProbeSidecar(
            path, &expected, reopened, reopenedMetadata, outError);
}

bool ReadSectorBakedObjectLightProbeSidecar(
        const std::string& path,
        const SectorBakedObjectLightProbeMetadata* expectedMetadata,
        std::vector<SectorBakedObjectLightProbe>& outProbes,
        SectorBakedObjectLightProbeMetadata& outMetadata,
        std::string& outError)
{
    outError.clear();
    outProbes.clear();
    outMetadata = {};

    if (path.empty()) {
        outError = "Object probe sidecar read failed: missing input path";
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        outError = "Object probe sidecar read failed: could not open input file";
        return false;
    }

    char magic[4] = {};
    input.read(magic, sizeof(magic));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(magic))
            || !std::equal(std::begin(magic), std::end(magic), std::begin(kObjectProbeSidecarMagic))) {
        outError = "Object probe sidecar read failed: bad magic";
        return false;
    }

    uint32_t version = 0;
    uint32_t headerBytes = 0;
    uint32_t probeCount = 0;
    float probeSpacingWorld = 0.0f;
    float probeLowerHeightWorld = 0.0f;
    float probeUpperHeightWorld = 0.0f;
    uint32_t sourceHashBytes = 0;
    uint64_t payloadByteCount = 0;
    uint64_t payloadChecksum = 0;
    if (!ReadU32LE(input, version)
            || !ReadU32LE(input, headerBytes)
            || !ReadU32LE(input, probeCount)
            || !ReadF32LE(input, probeSpacingWorld)
            || !ReadF32LE(input, probeLowerHeightWorld)
            || !ReadF32LE(input, probeUpperHeightWorld)
            || !ReadU32LE(input, sourceHashBytes)
            || !ReadU64LE(input, payloadByteCount)
            || !ReadU64LE(input, payloadChecksum)) {
        outError = "Object probe sidecar read failed: truncated header";
        return false;
    }

    if (version != static_cast<uint32_t>(kSectorBakedObjectLightProbeSidecarVersion)
            || sourceHashBytes == 0 || sourceHashBytes > 1024u
            || headerBytes != kObjectProbeFixedHeaderBytes + sourceHashBytes) {
        outError = "Object probe sidecar read failed: unsupported version";
        return false;
    }
    if (!std::isfinite(probeSpacingWorld)
            || !std::isfinite(probeLowerHeightWorld)
            || !std::isfinite(probeUpperHeightWorld)
            || probeLowerHeightWorld < 0.0f
            || probeUpperHeightWorld < probeLowerHeightWorld) {
        outError = "Object probe sidecar read failed: non-finite probe settings";
        return false;
    }
    if (probeCount > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        outError = "Object probe sidecar read failed: too many probes";
        return false;
    }
    constexpr uint64_t kProbePayloadRecordBytes = 92u;
    if (payloadByteCount != static_cast<uint64_t>(probeCount)
                    * kProbePayloadRecordBytes
            || payloadByteCount > std::numeric_limits<size_t>::max()) {
        outError = "Object probe sidecar read failed: invalid payload size";
        return false;
    }
    std::string sourceHash(sourceHashBytes, '\0');
    input.read(sourceHash.data(), static_cast<std::streamsize>(sourceHash.size()));
    std::vector<uint8_t> payloadBytes(static_cast<size_t>(payloadByteCount));
    input.read(reinterpret_cast<char*>(payloadBytes.data()),
            static_cast<std::streamsize>(payloadBytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(payloadBytes.size())
            || input.peek() != std::char_traits<char>::eof()
            || Fnv1aBytes(payloadBytes) != payloadChecksum) {
        outError = "Object probe sidecar read failed: truncated, trailing, or corrupt payload";
        return false;
    }
    if (expectedMetadata != nullptr) {
        if (expectedMetadata->version != 0
                && expectedMetadata->version != static_cast<int>(version)) {
            outError = "Object probe sidecar read failed: metadata version mismatch";
            return false;
        }
        if (expectedMetadata->count >= 0
                && expectedMetadata->count != static_cast<int>(probeCount)) {
            outError = "Object probe sidecar read failed: metadata count mismatch";
            return false;
        }
        if (!expectedMetadata->format.empty()
                && expectedMetadata->format != kSectorBakedObjectLightProbeSidecarFormat) {
            outError = "Object probe sidecar read failed: metadata format mismatch";
            return false;
        }
        if (!expectedMetadata->sourceHash.empty()
                && expectedMetadata->sourceHash != sourceHash) {
            outError = "Object probe sidecar read failed: metadata source hash mismatch";
            return false;
        }
        if ((expectedMetadata->probeSpacingWorld > 0.0f
                    && std::abs(expectedMetadata->probeSpacingWorld
                            - probeSpacingWorld) > 0.0001f)
                || std::abs(expectedMetadata->probeLowerHeightWorld
                        - probeLowerHeightWorld) > 0.0001f
                || std::abs(expectedMetadata->probeUpperHeightWorld
                        - probeUpperHeightWorld) > 0.0001f) {
            outError = "Object probe sidecar read failed: metadata settings mismatch";
            return false;
        }
    }

    const std::string payloadString(payloadBytes.begin(), payloadBytes.end());
    std::istringstream payloadInput(payloadString, std::ios::binary);
    std::vector<SectorBakedObjectLightProbe> probes;
    probes.reserve(probeCount);
    for (uint32_t probeIndex = 0; probeIndex < probeCount; ++probeIndex) {
        SectorBakedObjectLightProbe probe;
        int32_t sectorId = 0;
        uint32_t layer = 0;
        if (!ReadI32LE(payloadInput, sectorId)
                || !ReadU32LE(payloadInput, layer)
                || !ReadProbeVector(payloadInput, probe.position)) {
            outError = "Object probe sidecar read failed: truncated probe record";
            return false;
        }
        probe.sectorId = static_cast<int>(sectorId);
        if (layer > static_cast<uint32_t>(SectorBakedObjectLightProbeLayer::Upper)) {
            outError = "Object probe sidecar read failed: invalid probe layer";
            return false;
        }
        probe.layer = static_cast<SectorBakedObjectLightProbeLayer>(layer);
        if (!IsFiniteVector3(probe.position)) {
            outError = "Object probe sidecar read failed: non-finite probe position";
            return false;
        }
        for (Vector3& cubeFace : probe.ambientCube) {
            if (!ReadProbeVector(payloadInput, cubeFace)) {
                outError = "Object probe sidecar read failed: truncated probe ambient cube";
                return false;
            }
            if (!IsFiniteVector3(cubeFace) || cubeFace.x < 0.0f
                    || cubeFace.y < 0.0f || cubeFace.z < 0.0f) {
                outError = "Object probe sidecar read failed: invalid ambient cube value";
                return false;
            }
        }
        probes.push_back(probe);
    }

    outMetadata.path = path;
    outMetadata.version = static_cast<int>(version);
    outMetadata.sourceHash = sourceHash;
    outMetadata.count = static_cast<int>(probeCount);
    outMetadata.probeSpacingWorld = probeSpacingWorld;
    outMetadata.probeLowerHeightWorld = probeLowerHeightWorld;
    outMetadata.probeUpperHeightWorld = probeUpperHeightWorld;
    outMetadata.format = kSectorBakedObjectLightProbeSidecarFormat;
    for (const SectorBakedObjectLightProbe& probe : probes) {
        for (const Vector3& cubeFace : probe.ambientCube) {
            AccumulateStatistics(outMetadata.storedStatistics, cubeFace, 0.0f);
        }
    }
    outProbes = std::move(probes);
    return true;
}

bool LoadSectorBakedObjectLightProbeRuntimeData(
        const SectorTopologyMap& map,
        SectorBakedObjectLightProbeRuntimeData& outData,
        std::string& outError)
{
    outError.clear();
    outData = {};

    const SectorBakedObjectLightProbeMetadata& metadata = map.bakedLightmap.objectProbes;
    if (metadata.path.empty()) {
        outError = "Object probe runtime load skipped: missing probe metadata";
        return false;
    }

    if (metadata.version != kSectorBakedObjectLightProbeSidecarVersion
            || metadata.sourceHash.empty()
            || metadata.count < 0
            || metadata.probeSpacingWorld <= 0.0f
            || metadata.probeLowerHeightWorld < 0.0f
            || metadata.probeUpperHeightWorld
                    < metadata.probeLowerHeightWorld
            || metadata.format != kSectorBakedObjectLightProbeSidecarFormat) {
        outError = "Object probe runtime load failed: invalid probe metadata";
        return false;
    }

    if (metadata.sourceHash != ComputeSectorLightmapSourceHash(map)) {
        outError = "Object probe runtime load failed: stale source hash";
        return false;
    }

    const std::string resolvedPath = ResolveSectorAssetPath(metadata.path);
    std::vector<SectorBakedObjectLightProbe> probes;
    SectorBakedObjectLightProbeMetadata loadedMetadata;
    if (!ReadSectorBakedObjectLightProbeSidecar(resolvedPath, &metadata, probes, loadedMetadata, outError)) {
        if (outError.empty()) {
            outError = "Object probe runtime load failed: could not read sidecar";
        }
        return false;
    }

    std::sort(probes.begin(), probes.end(), [](const SectorBakedObjectLightProbe& a, const SectorBakedObjectLightProbe& b) {
        if (a.sectorId != b.sectorId) return a.sectorId < b.sectorId;
        return static_cast<unsigned int>(a.layer)
                < static_cast<unsigned int>(b.layer);
    });

    std::vector<SectorBakedObjectLightProbeSectorRange> sectorRanges;
    sectorRanges.reserve(probes.size());
    for (size_t begin = 0; begin < probes.size();) {
        const int sectorId = probes[begin].sectorId;
        const SectorBakedObjectLightProbeLayer layer = probes[begin].layer;
        size_t end = begin + 1;
        while (end < probes.size()
                && probes[end].sectorId == sectorId
                && probes[end].layer == layer) {
            ++end;
        }

        SectorBakedObjectLightProbeSectorRange range;
        range.sectorId = sectorId;
        range.begin = static_cast<int>(begin);
        range.count = static_cast<int>(end - begin);
        range.layer = layer;
        sectorRanges.push_back(range);
        begin = end;
    }

    loadedMetadata.path = metadata.path;
    loadedMetadata.sourceHash = metadata.sourceHash;
    outData.probes = std::move(probes);
    outData.sectorRanges = std::move(sectorRanges);
    outData.metadata = std::move(loadedMetadata);
    BuildSectorBakedObjectLightProbePortalAdjacency(map, outData);
    return true;
}

void BuildSectorBakedObjectLightProbePortalAdjacency(
        const SectorTopologyMap& map,
        SectorBakedObjectLightProbeRuntimeData& outData)
{
    struct DirectedPortal {
        int sectorId = 0;
        SectorBakedObjectLightProbePortal portal;
    };

    outData.portals.clear();
    outData.portalRanges.clear();
    outData.portalAdjacencyPrepared = true;

    const SectorTopologyIndexes indexes = BuildSectorTopologyIndexes(map);
    auto uniqueIndex = [](const auto& index, int id) -> const size_t* {
        const auto it = index.find(id);
        return it != index.end() && it->second.size() == 1
                ? &it->second.front()
                : nullptr;
    };

    std::vector<DirectedPortal> directed;
    directed.reserve(map.lineDefs.size() * 2);
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        const size_t* frontIndex = uniqueIndex(
                indexes.sideDefIndicesById,
                lineDef.frontSideDefId);
        const size_t* backIndex = uniqueIndex(
                indexes.sideDefIndicesById,
                lineDef.backSideDefId);
        const size_t* startIndex = uniqueIndex(
                indexes.vertexIndicesById,
                lineDef.startVertexId);
        const size_t* endIndex = uniqueIndex(
                indexes.vertexIndicesById,
                lineDef.endVertexId);
        if (frontIndex == nullptr || backIndex == nullptr
                || startIndex == nullptr || endIndex == nullptr
                || *frontIndex >= map.sideDefs.size()
                || *backIndex >= map.sideDefs.size()
                || *startIndex >= map.vertices.size()
                || *endIndex >= map.vertices.size()) {
            continue;
        }

        const SectorTopologySideDef& front = map.sideDefs[*frontIndex];
        const SectorTopologySideDef& back = map.sideDefs[*backIndex];
        if (front.lineDefId != lineDef.id || back.lineDefId != lineDef.id
                || front.sectorId == back.sectorId) {
            continue;
        }
        const size_t* frontSectorIndex = uniqueIndex(
                indexes.sectorIndicesById,
                front.sectorId);
        const size_t* backSectorIndex = uniqueIndex(
                indexes.sectorIndicesById,
                back.sectorId);
        if (frontSectorIndex == nullptr || backSectorIndex == nullptr
                || *frontSectorIndex >= map.sectors.size()
                || *backSectorIndex >= map.sectors.size()) {
            continue;
        }

        const SectorTopologySector& frontSector = map.sectors[*frontSectorIndex];
        const SectorTopologySector& backSector = map.sectors[*backSectorIndex];
        const float openBottom = std::max(
                SectorAuthoringToWorldDistance(frontSector.floorZ),
                SectorAuthoringToWorldDistance(backSector.floorZ));
        const float openTop = std::min(
                SectorAuthoringToWorldDistance(frontSector.ceilingZ),
                SectorAuthoringToWorldDistance(backSector.ceilingZ));
        if (!(openBottom < openTop)) {
            continue;
        }

        const SectorTopologyVertex& start = map.vertices[*startIndex];
        const SectorTopologyVertex& end = map.vertices[*endIndex];
        const Vector2 startWorld = SectorCoordToWorldPosition2(start.x, start.y);
        const Vector2 endWorld = SectorCoordToWorldPosition2(end.x, end.y);
        directed.push_back(DirectedPortal{
                front.sectorId,
                SectorBakedObjectLightProbePortal{
                        back.sectorId,
                        startWorld,
                        endWorld}});
        directed.push_back(DirectedPortal{
                back.sectorId,
                SectorBakedObjectLightProbePortal{
                        front.sectorId,
                        startWorld,
                        endWorld}});
    }

    std::stable_sort(
            directed.begin(),
            directed.end(),
            [](const DirectedPortal& a, const DirectedPortal& b) {
                return a.sectorId < b.sectorId;
            });
    outData.portals.reserve(directed.size());
    outData.portalRanges.reserve(directed.size());
    for (size_t begin = 0; begin < directed.size();) {
        const int sectorId = directed[begin].sectorId;
        size_t end = begin + 1;
        while (end < directed.size() && directed[end].sectorId == sectorId) {
            ++end;
        }
        const int portalBegin = static_cast<int>(outData.portals.size());
        for (size_t index = begin; index < end; ++index) {
            outData.portals.push_back(directed[index].portal);
        }
        outData.portalRanges.push_back(SectorBakedObjectLightProbePortalRange{
                sectorId,
                portalBegin,
                static_cast<int>(end - begin)});
        begin = end;
    }
}

Vector3 EvaluateBakedObjectAmbientCubeLighting(
        const BakedObjectLightingSample& sample,
        Vector3 worldNormal)
{
    if (!IsFiniteVector3(worldNormal)
            || Vector3LengthSqr(worldNormal) <= 0.000001f) {
        worldNormal = Vector3{0.0f, 1.0f, 0.0f};
    } else {
        worldNormal = Vector3Normalize(worldNormal);
    }

    const Vector3 weights{
            worldNormal.x * worldNormal.x,
            worldNormal.y * worldNormal.y,
            worldNormal.z * worldNormal.z};
    const Vector3 xLighting = sample.ambientCube[worldNormal.x >= 0.0f ? 0 : 1];
    const Vector3 yLighting = sample.ambientCube[worldNormal.y >= 0.0f ? 2 : 3];
    const Vector3 zLighting = sample.ambientCube[worldNormal.z >= 0.0f ? 4 : 5];
    return Vector3Add(
            Vector3Add(
                    Vector3Scale(xLighting, weights.x),
                    Vector3Scale(yLighting, weights.y)),
            Vector3Scale(zLighting, weights.z));
}

BakedObjectLightingSample ResolveBakedObjectLightingVerticalSample(
        const BakedObjectLightingVerticalSample& sample,
        float worldHeight)
{
    if (!sample.lower.valid && sample.upper.valid) return sample.upper;
    if (sample.lower.valid && !sample.upper.valid) return sample.lower;
    const float heightRange = sample.upperHeightWorld - sample.lowerHeightWorld;
    if (!std::isfinite(heightRange)
            || heightRange <= 0.0001f
            || !std::isfinite(worldHeight)) {
        return sample.lower;
    }

    const float blend = std::clamp(
            (worldHeight - sample.lowerHeightWorld) / heightRange,
            0.0f,
            1.0f);
    BakedObjectLightingSample resolved;
    resolved.valid = sample.lower.valid || sample.upper.valid;
    for (int face = 0; face < 6; ++face) {
        resolved.ambientCube[face] = Vector3Lerp(
                sample.lower.ambientCube[face],
                sample.upper.ambientCube[face],
                blend);
    }
    return resolved;
}

BakedObjectLightingVerticalSample SampleBakedObjectLightingVertical(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        Vector3 worldPosition,
        int preferredSectorId,
        const SectorTopologyMap* mapForFallback)
{
    if (!probes.probes.empty()) {
        const SectorBakedObjectLightProbeSectorRange* preferredLower =
                FindProbeSectorRange(
                        probes,
                        preferredSectorId,
                        SectorBakedObjectLightProbeLayer::Lower);
        const SectorBakedObjectLightProbeSectorRange* preferredUpper =
                FindProbeSectorRange(
                        probes,
                        preferredSectorId,
                        SectorBakedObjectLightProbeLayer::Upper);
        const bool hasPreferredLayer = preferredLower != nullptr
                || preferredUpper != nullptr;

        auto sampleLayer = [&](SectorBakedObjectLightProbeLayer layer,
                                   const SectorBakedObjectLightProbeSectorRange* preferredRange) {
            ObjectProbeSelection selection;
            if (preferredRange != nullptr && preferredRange->count > 0) {
                StreamObjectLightProbeRange(
                        probes,
                        worldPosition,
                        preferredRange->begin,
                        preferredRange->count,
                        selection);

                if (mapForFallback != nullptr
                        && probes.portalAdjacencyPrepared) {
                    int adjacentSectorIds[kObjectProbeMaxAdjacentBlendSectors] = {};
                    int adjacentSectorCount = 0;
                    CollectAdjacentObjectProbeSectorIdsNearPortals(
                            probes,
                            worldPosition,
                            preferredSectorId,
                            adjacentSectorIds,
                            adjacentSectorCount);
                    for (int index = 0; index < adjacentSectorCount; ++index) {
                        const SectorBakedObjectLightProbeSectorRange* adjacentRange =
                                FindProbeSectorRange(
                                        probes,
                                        adjacentSectorIds[index],
                                        layer);
                        if (adjacentRange != nullptr && adjacentRange->count > 0) {
                            StreamObjectLightProbeRange(
                                    probes,
                                    worldPosition,
                                    adjacentRange->begin,
                                    adjacentRange->count,
                                    selection);
                        }
                    }
                }
            } else if (!hasPreferredLayer) {
                for (const SectorBakedObjectLightProbeSectorRange& range :
                        probes.sectorRanges) {
                    if (range.layer == layer && range.count > 0) {
                        StreamObjectLightProbeRange(
                                probes,
                                worldPosition,
                                range.begin,
                                range.count,
                                selection);
                    }
                }
            }
            return SampleSelectedObjectLightProbes(probes, selection);
        };

        SelectedObjectProbeLayerSample lower = sampleLayer(
                SectorBakedObjectLightProbeLayer::Lower,
                preferredLower);
        SelectedObjectProbeLayerSample upper = sampleLayer(
                SectorBakedObjectLightProbeLayer::Upper,
                preferredUpper);
        if (!lower.lighting.valid && upper.lighting.valid) lower = upper;
        if (lower.lighting.valid && !upper.lighting.valid) upper = lower;
        if (lower.lighting.valid || upper.lighting.valid) {
            return BakedObjectLightingVerticalSample{
                    lower.lighting,
                    upper.lighting,
                    lower.heightWorld,
                    upper.heightWorld};
        }
    }

    const BakedObjectLightingSample fallback = mapForFallback != nullptr
                    && FindSectorTopologySector(
                            *mapForFallback,
                            preferredSectorId) != nullptr
            ? MakeSectorAmbientObjectLightingSample(
                    *mapForFallback,
                    preferredSectorId)
            : MakeNeutralObjectLightingSample();
    return BakedObjectLightingVerticalSample{
            fallback,
            fallback,
            worldPosition.y,
            worldPosition.y};
}

BakedObjectLightingSample SampleBakedObjectLighting(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        Vector3 worldPosition,
        int preferredSectorId,
        const SectorTopologyMap* mapForFallback)
{
    return ResolveBakedObjectLightingVerticalSample(
            SampleBakedObjectLightingVertical(
                    probes,
                    worldPosition,
                    preferredSectorId,
                    mapForFallback),
            worldPosition.y);
}

std::string MakeSectorLightmapPathForMapPath(const std::string& mapPath)
{
    std::filesystem::path path(mapPath);
    path.replace_extension(".lightmap.bin");
    return MakeSectorAssetRelativePath(path.generic_string());
}

std::string MakeSectorLightmapAtlasPath(
        const std::string& primaryPath,
        int atlasIndex)
{
    if (atlasIndex <= 0 || primaryPath.empty()) {
        return primaryPath;
    }
    const std::filesystem::path primary(primaryPath);
    const std::string filename = primary.stem().string()
            + "." + std::to_string(atlasIndex)
            + primary.extension().string();
    return (primary.parent_path() / filename).generic_string();
}

std::vector<SectorLightmapAtlasMetadata> GetSectorLightmapAtlases(
        const SectorLightmapMetadata& metadata)
{
    std::vector<SectorLightmapAtlasMetadata> atlases;
    if (!metadata.path.empty()) {
        atlases.push_back(SectorLightmapAtlasMetadata{
                metadata.path,
                metadata.width,
                metadata.height});
    }
    atlases.insert(
            atlases.end(),
            metadata.additionalAtlases.begin(),
            metadata.additionalAtlases.end());
    return atlases;
}

std::string MakeSectorObjectProbeSidecarPathForLightmapPath(const std::string& lightmapPath)
{
    std::filesystem::path path(lightmapPath);
    path.replace_extension(".object_probes.bin");
    return path.generic_string();
}

bool BuildSectorLightmapLayout(
        const SectorTopologyMap& map,
        SectorLightmapLayout& outLayout,
        std::string& outError)
{
    outError.clear();

    SectorGeneratedGeometry geometry;
    if (!BuildSectorGeneratedGeometry(map, geometry, &outError)) {
        if (outError.empty()) {
            outError = "Bake failed: no generated topology sector surfaces";
        } else {
            outError = "Bake failed: " + outError;
        }
        return false;
    }

    const SectorLightmapBakeQualityParameters quality =
            ResolveSectorLightmapBakeQuality(map.lightmapSettings.qualityPreset);
    return BuildSectorLightmapLayoutFromGeometry(
            geometry,
            quality.texelsPerWorldUnit,
            outLayout,
            outError);
}

bool BakeSectorLightmap(
        const SectorTopologyMap& map,
        const SectorLightmapLayout& layout,
        const char* outputPath,
        SectorLightmapBakeResult& outResult,
        std::string& outError)
{
    SectorLightmapBakeCallbacks callbacks;
    return BakeSectorLightmap(map, layout, outputPath, callbacks, outResult, outError);
}

template<typename MapT>
bool BakeSectorLightmapForMap(
        const MapT& map,
        const SectorLightmapLayout& layout,
        SectorStaticModelLightmapData* staticModels,
        const char* outputPath,
        const SectorLightmapBakeCallbacks& callbacks,
        SectorLightmapBakeResult& outResult,
        std::string& outError)
{
    outResult = SectorLightmapBakeResult{};
    outError.clear();
    const SectorLightmapBakeQualityPreset qualityPreset =
            NormalizeSectorLightmapBakeQualityPreset(
                    map.lightmapSettings.qualityPreset);
    const SectorLightmapBakeQualityParameters quality =
            ResolveSectorLightmapBakeQuality(qualityPreset);
    outResult.qualityPreset = qualityPreset;
    outResult.qualityParameters = quality;
    if (outputPath == nullptr || outputPath[0] == '\0') {
        outError = "Bake failed: missing output path";
        return false;
    }

    SectorGeneratedGeometry geometry;
    if (!BuildLightmapGeneratedGeometryForBake(map, geometry, outError)) {
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
    int atlasCount = std::max(1, layout.atlasCount);
    for (const SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex >= 0) {
            if (chart.atlasIndex < 0) {
                outError = "Bake failed: lightmap chart has no atlas assignment";
                return false;
            }
            atlasCount = std::max(atlasCount, chart.atlasIndex + 1);
        }
    }
    if (staticModels != nullptr) {
        for (const SectorStaticModelLightmapObject& object : staticModels->objects) {
            for (const SectorStaticModelLightmapMeshPlacement& placement
                    : object.meshPlacements) {
                if (placement.atlasIndex < 0) {
                    outError = "Bake failed: static model lightmap chart has no atlas assignment";
                    return false;
                }
                atlasCount = std::max(atlasCount, placement.atlasIndex + 1);
            }
        }
    }
    if (width <= 0 || height <= 0
            || static_cast<size_t>(width) > std::numeric_limits<size_t>::max()
                    / static_cast<size_t>(height)) {
        outError = "Bake failed: invalid lightmap atlas dimensions";
        return false;
    }
    const size_t atlasPixelCount = static_cast<size_t>(width)
            * static_cast<size_t>(height);
    if (static_cast<size_t>(atlasCount)
            > std::numeric_limits<size_t>::max() / atlasPixelCount) {
        outError = "Bake failed: lightmap atlas storage is too large";
        return false;
    }
    const size_t totalPixelCount = atlasPixelCount
            * static_cast<size_t>(atlasCount);
    outResult.width = width;
    outResult.height = height;
    outResult.atlases.reserve(static_cast<size_t>(atlasCount));
    for (int atlasIndex = 0; atlasIndex < atlasCount; ++atlasIndex) {
        outResult.atlases.push_back(SectorLightmapAtlasMetadata{
                MakeSectorLightmapAtlasPath(outputPath, atlasIndex),
                width,
                height});
    }
    const auto removeAtlasOutputs = [&]() {
        for (const SectorLightmapAtlasMetadata& atlas : outResult.atlases) {
            RemoveFileIfExists(atlas.path);
        }
    };
    ReportProgress(callbacks, SectorLightmapBakePhase::Preparing, 0, 1);
    const std::string artifactSourceHash = ComputeSectorLightmapSourceHash(map);
    std::vector<Vector4> pixels(totalPixelCount, Vector4{0.0f, 0.0f, 0.0f, 1.0f});
    std::vector<Vector4> directionalPixels(
            totalPixelCount, Vector4{0.5f, 0.5f, 1.0f, 0.0f});
    std::vector<Vector3> directLightingFloat(totalPixelCount, Vector3{});
    std::vector<Vector3> directDirectionMoments(totalPixelCount, Vector3{});
    std::vector<Vector3> indirectLightingFloat(totalPixelCount, Vector3{});
    std::vector<float> ambientOcclusionFloat(totalPixelCount, 1.0f);
    std::vector<unsigned char> validChartTexel(totalPixelCount, 0);
    std::vector<BakeTexel> bakeTexels;
    const std::vector<SectorLightmapAlphaOccluderTriangle> alphaOccluders =
            CollectSectorLightmapAlphaOccluders(geometry);
    SectorLightmapAlphaMaskCache alphaMaskCache;
    const std::vector<BakeTriangle> triangles =
            BuildBakeTriangles(geometry, layout, staticModels);
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
    const float aoRadius = std::clamp(
            SectorAuthoringToWorldDistance(map.lightmapSettings.ambientOcclusionRadius),
            0.05f,
            16.0f
    );
    const float aoStrength = std::clamp(map.lightmapSettings.ambientOcclusionStrength, 0.0f, 1.0f);
    const float indirectBounceRadius = std::clamp(
            SectorAuthoringToWorldDistance(map.lightmapSettings.indirectBounceRadius),
            0.05f,
            16.0f
    );
    const float indirectBounceStrength = std::clamp(map.lightmapSettings.indirectBounceStrength, 0.0f, 1.0f);
    std::vector<LightmapWorldPointLight> worldLights;
    worldLights.reserve(map.staticLights.size());
    for (const auto& light : map.staticLights) {
        worldLights.push_back(MakeWorldSpaceLight(light));
    }
    std::vector<LightmapWorldSpotLight> worldSpotLights;
    worldSpotLights.reserve(map.staticSpotLights.size());
    for (const auto& light : map.staticSpotLights) {
        worldSpotLights.push_back(MakeWorldSpaceLight(light));
    }
    std::vector<LightmapWorldRectLight> worldRectLights;
    worldRectLights.reserve(map.staticRectLights.size());
    for (const auto& light : map.staticRectLights) {
        worldRectLights.push_back(MakeWorldSpaceLight(light));
    }
    const LightmapWorldDirectionalLight directionalLight =
            MakeWorldSpaceDirectionalLight(map.directionalLight);
    const float directionalShadowMaxDistance = BvhSceneDiagonalWithMargin(bvh);
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

                const size_t pixelIndex = static_cast<size_t>(chart.atlasIndex)
                        * atlasPixelCount
                        + static_cast<size_t>(y * width + x);
                bakeTexels.push_back(BakeTexel{
                        chart.atlasIndex,
                        x,
                        y,
                        pixelIndex,
                        surface.ref,
                        chart.surfaceIndex,
                        hit.triangleIndex,
                        hit.position,
                        hit.geometricNormal,
                        hit.geometricNormal
                });
                validChartTexel[pixelIndex] = 1;
            }
        }
    }
    if (staticModels != nullptr) {
        int sourceSurfaceIndex = static_cast<int>(geometry.surfaces.size());
        for (const SectorStaticModelLightmapObject& object : staticModels->objects) {
            if (object.modelIndex < 0
                    || object.modelIndex
                            >= static_cast<int>(staticModels->models.size())) {
                outError = "Bake failed: invalid prepared static model object "
                        + std::to_string(object.objectId);
                return false;
            }
            const SectorStaticModelLightmapModel& model =
                    staticModels->models[static_cast<size_t>(object.modelIndex)];
            for (size_t meshIndex = 0;
                    meshIndex < model.meshes.size();
                    ++meshIndex, ++sourceSurfaceIndex) {
                if (meshIndex >= object.meshPlacements.size()) {
                    outError = "Bake failed: missing lightmap chart for static model object "
                            + std::to_string(object.objectId);
                    return false;
                }
                const SectorStaticModelLightmapMesh& mesh =
                        model.meshes[meshIndex];
                const SectorStaticModelLightmapMeshPlacement& placement =
                        object.meshPlacements[meshIndex];
                allocatedChartRectanglePixels +=
                        placement.width * placement.height;
                const SectorGeneratedSurfaceRef surfaceRef{
                        SectorGeneratedSurfaceKind::Middle,
                        object.containingSectorId,
                        -1,
                        -1,
                        SectorTopologySideKind::Front};
                for (int y = placement.usableY;
                        y < placement.usableY + placement.usableHeight;
                        ++y) {
                    for (int x = placement.usableX;
                            x < placement.usableX + placement.usableWidth;
                            ++x) {
                        const Vector2 localUv{
                                (static_cast<float>(x - placement.usableX) + 0.5f)
                                        / static_cast<float>(placement.usableWidth),
                                (static_cast<float>(y - placement.usableY) + 0.5f)
                                        / static_cast<float>(placement.usableHeight)};
                        RasterHit hit;
                        if (!RasterizeStaticModelMeshPoint(
                                    mesh,
                                    object,
                                    localUv,
                                    hit)) {
                            continue;
                        }
                        const size_t pixelIndex =
                                static_cast<size_t>(placement.atlasIndex)
                                        * atlasPixelCount
                                + static_cast<size_t>(y * width + x);
                        bakeTexels.push_back(BakeTexel{
                                placement.atlasIndex,
                                x,
                                y,
                                pixelIndex,
                                surfaceRef,
                                sourceSurfaceIndex,
                                hit.triangleIndex,
                                hit.position,
                                hit.normal,
                                hit.geometricNormal});
                        validChartTexel[pixelIndex] = 1;
                    }
                }
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
    if (!worldLights.empty() || !worldSpotLights.empty() || !worldRectLights.empty()
            || directionalLight.enabled) {
        for (const BakeTexel& texel : bakeTexels) {
            RasterHit hit;
            hit.hit = true;
            hit.position = texel.position;
            hit.normal = texel.normal;
            hit.geometricNormal = texel.geometricNormal;
            hit.triangleIndex = texel.triangleIndex;

            Vector3 direct{};
            Vector3 directionMoment{};
            for (const LightmapWorldPointLight& light : worldLights) {
                const DirectLightEvaluation evaluation = EvaluateDirectLight(
                                map,
                                light,
                                hit,
                                texel.surfaceRef,
                                texel.sourceSurfaceIndex,
                                bvh,
                                triangles,
                                alphaOccluders,
                                alphaMaskCache,
                                quality.directSoftShadowSampleCount,
                                stats);
                direct = Vector3Add(direct, evaluation.radiance);
                directionMoment = Vector3Add(
                        directionMoment, evaluation.directionMoment);
            }
            for (const LightmapWorldSpotLight& light : worldSpotLights) {
                const DirectLightEvaluation evaluation = EvaluateDirectLight(
                                map,
                                light,
                                hit,
                                texel.surfaceRef,
                                texel.sourceSurfaceIndex,
                                bvh,
                                triangles,
                                alphaOccluders,
                                alphaMaskCache,
                                quality.directSoftShadowSampleCount,
                                stats);
                direct = Vector3Add(direct, evaluation.radiance);
                directionMoment = Vector3Add(
                        directionMoment, evaluation.directionMoment);
            }
            for (const LightmapWorldRectLight& light : worldRectLights) {
                const DirectLightEvaluation evaluation = EvaluateDirectLight(
                        map, light, hit, texel.surfaceRef, texel.sourceSurfaceIndex,
                        bvh, triangles, alphaOccluders, alphaMaskCache,
                        quality.directSoftShadowSampleCount, stats);
                direct = Vector3Add(direct, evaluation.radiance);
                directionMoment = Vector3Add(
                        directionMoment, evaluation.directionMoment);
            }
            if (IsSkyOwnedLightmapSurface(map, texel.surfaceRef)) {
                const DirectLightEvaluation evaluation = EvaluateDirectionalLight(
                                map,
                                directionalLight,
                                hit,
                                texel.surfaceRef,
                                texel.sourceSurfaceIndex,
                                directionalShadowMaxDistance,
                                bvh,
                                triangles,
                                alphaOccluders,
                                alphaMaskCache,
                                stats);
                direct = Vector3Add(direct, evaluation.radiance);
                directionMoment = Vector3Add(
                        directionMoment, evaluation.directionMoment);
            }
            directLightingFloat[texel.pixelIndex] = direct;
            directDirectionMoments[texel.pixelIndex] = directionMoment;
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
            hit.normal = texel.geometricNormal;
            hit.geometricNormal = texel.geometricNormal;
            hit.triangleIndex = texel.triangleIndex;
            ambientOcclusionFloat[texel.pixelIndex] = BakeAmbientOcclusion(
                    hit,
                    texel.surfaceRef,
                    texel.sourceSurfaceIndex,
                    aoRadius,
                    aoStrength,
                    quality.ambientOcclusionSampleCount,
                    bvh,
                    triangles,
                    stats);
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
            DilateChartFloat(chart, directSampleFloat, directSampleValid, width, height);
        }
        if (staticModels != nullptr) {
            for (const SectorStaticModelLightmapObject& object : staticModels->objects) {
                for (const SectorStaticModelLightmapMeshPlacement& placement
                        : object.meshPlacements) {
                    const SectorLightmapChart chart = PlacementAsChart(placement);
                    DilateChartFloat(
                            chart,
                            directSampleFloat,
                            directSampleValid,
                            width,
                            height);
                }
            }
        }

        completedTexels = 0;
        for (const BakeTexel& texel : bakeTexels) {
            const Vector3 origin = Vector3Add(
                    texel.position,
                    Vector3Scale(texel.geometricNormal, RayOriginEpsilon));
            Vector3 gathered{};
            for (int i = 0; i < quality.indirectBounceSampleCount; ++i) {
                const Vector3 direction = CosineHemisphereSample(
                        texel.geometricNormal,
                        i,
                        quality.indirectBounceSampleCount);
                const RayHit rayHit = TraceRay(
                        origin,
                        direction,
                        indirectBounceRadius,
                        texel.surfaceRef,
                        texel.sourceSurfaceIndex,
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
                        rayHit.lightmapAtlasIndex,
                        rayHit.lightmapUv
                );
                const float distanceT = std::clamp(1.0f - rayHit.distance / indirectBounceRadius, 0.0f, 1.0f);
                const float distanceWeight = distanceT * distanceT;
                const float hitFacing = std::max(Vector3DotProduct(rayHit.normal, Vector3Negate(direction)), 0.0f);
                const float scale = kNeutralBounceAlbedo * distanceWeight * hitFacing;
                gathered = Vector3Add(gathered, Vector3Scale(sampledDirect, scale));
            }

            const float averageScale = indirectBounceStrength
                    / static_cast<float>(quality.indirectBounceSampleCount);
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
    size_t staticChartCount = 0;
    if (staticModels != nullptr) {
        for (const SectorStaticModelLightmapObject& object : staticModels->objects) {
            staticChartCount += object.meshPlacements.size();
        }
    }
    const uint32_t exportWorkTotal = static_cast<uint32_t>(
            layout.charts.size() + staticChartCount + bakeTexels.size()
            + static_cast<size_t>(atlasCount));
    ReportProgress(
            callbacks,
            SectorLightmapBakePhase::DilatingAndEncoding,
            0,
            exportWorkTotal);
    std::vector<unsigned char> exportValid = validChartTexel;
    std::vector<unsigned char> directionalExportValid = validChartTexel;
    completedTexels = 0;
    for (const BakeTexel& texel : bakeTexels) {
        const Vector3 finalRgb = Vector3Add(
                directLightingFloat[texel.pixelIndex],
                indirectLightingFloat[texel.pixelIndex]);
        if (!IsFiniteVector3(finalRgb) || finalRgb.x < 0.0f
                || finalRgb.y < 0.0f || finalRgb.z < 0.0f) {
            outError = "Bake failed: non-finite or negative baked radiance";
            return false;
        }
        pixels[texel.pixelIndex] = Vector4{
                finalRgb.x,
                finalRgb.y,
                finalRgb.z,
                std::clamp(ambientOcclusionFloat[texel.pixelIndex], 0.0f, 1.0f)};
        const Vector3 directionMoment = directDirectionMoments[texel.pixelIndex];
        const float directionLength = Vector3Length(directionMoment);
        const float directLuminance = std::max(
                LinearLuminance(directLightingFloat[texel.pixelIndex]), 0.0f);
        const float totalLuminance = directLuminance + std::max(
                LinearLuminance(indirectLightingFloat[texel.pixelIndex]), 0.0f);
        if (directionLength > BakeEpsilon && directLuminance > BakeEpsilon
                && totalLuminance > BakeEpsilon) {
            const Vector3 direction = Vector3Scale(
                    directionMoment, 1.0f / directionLength);
            directionalPixels[texel.pixelIndex] = Vector4{
                    direction.x * 0.5f + 0.5f,
                    direction.y * 0.5f + 0.5f,
                    direction.z * 0.5f + 0.5f,
                    std::clamp(directLuminance / totalLuminance, 0.0f, 1.0f)};
        }
        ++completedTexels;
        if ((completedTexels % kSectorLightmapProgressChunk) == 0) {
            ReportProgress(
                    callbacks,
                    SectorLightmapBakePhase::DilatingAndEncoding,
                    completedTexels,
                    exportWorkTotal);
            if (CheckBakeCancelled(callbacks, outError)) {
                return false;
            }
        }
    }
    uint32_t completedExportWork = static_cast<uint32_t>(bakeTexels.size());
    for (const SectorLightmapChart& chart : layout.charts) {
        DilateChart(chart, pixels, exportValid, width, height);
        DilateChart(
                chart,
                directionalPixels,
                directionalExportValid,
                width,
                height);
        ++completedExportWork;
        ReportProgress(
                callbacks,
                SectorLightmapBakePhase::DilatingAndEncoding,
                completedExportWork,
                exportWorkTotal);
        if (CheckBakeCancelled(callbacks, outError)) {
            return false;
        }
    }
    if (staticModels != nullptr) {
        for (const SectorStaticModelLightmapObject& object : staticModels->objects) {
            for (const SectorStaticModelLightmapMeshPlacement& placement
                    : object.meshPlacements) {
                DilateChart(
                        PlacementAsChart(placement),
                        pixels,
                        exportValid,
                        width,
                        height);
                DilateChart(
                        PlacementAsChart(placement),
                        directionalPixels,
                        directionalExportValid,
                        width,
                        height);
                ++completedExportWork;
                ReportProgress(
                        callbacks,
                        SectorLightmapBakePhase::DilatingAndEncoding,
                        completedExportWork,
                        exportWorkTotal);
                if (CheckBakeCancelled(callbacks, outError)) {
                    return false;
                }
            }
        }
    }

    const std::filesystem::path output(outputPath);
    std::error_code ec;
    if (!output.parent_path().empty()) {
        std::filesystem::create_directories(output.parent_path(), ec);
    }
    if (ec) {
        outError = TextFormat("Bake failed: could not create output directory: %s", ec.message().c_str());
        return false;
    }

    for (int atlasIndex = 0; atlasIndex < atlasCount; ++atlasIndex) {
        const std::string& atlasPath =
                outResult.atlases[static_cast<size_t>(atlasIndex)].path;
        SectorIlluminationStatistics preEncodeStatistics;
        SectorIlluminationStatistics storedStatistics;
        if (!WriteSectorLightmapArtifact(
                    atlasPath,
                    width,
                    height,
                    pixels.data() + static_cast<size_t>(atlasIndex) * atlasPixelCount,
                    directionalPixels.data()
                            + static_cast<size_t>(atlasIndex) * atlasPixelCount,
                    atlasPixelCount,
                    artifactSourceHash,
                    preEncodeStatistics,
                    storedStatistics,
                    outError)) {
            outError = TextFormat("Bake failed: %s", outError.c_str());
            removeAtlasOutputs();
            return false;
        }
        outResult.atlases[static_cast<size_t>(atlasIndex)].storedStatistics =
                storedStatistics;
        MergeStatistics(outResult.preEncodeAtlasStatistics, preEncodeStatistics);
        MergeStatistics(outResult.storedAtlasStatistics, storedStatistics);
        ++completedExportWork;
        ReportProgress(
                callbacks,
                SectorLightmapBakePhase::DilatingAndEncoding,
                completedExportWork,
                exportWorkTotal);
        if (CheckBakeCancelled(callbacks, outError)) {
            removeAtlasOutputs();
            return false;
        }
    }
    ReportProgress(
            callbacks,
            SectorLightmapBakePhase::DilatingAndEncoding,
            exportWorkTotal,
            exportWorkTotal);
    const auto exportEnd = Clock::now();
    if (CheckBakeCancelled(callbacks, outError)) {
        removeAtlasOutputs();
        return false;
    }

    const float objectProbeSpacingWorld = std::clamp(map.lightmapSettings.objectProbeSpacingWorld, 0.25f, 128.0f);
    float objectProbeLowerHeightWorld = std::clamp(
            map.lightmapSettings.objectProbeLowerHeightWorld, 0.0f, 16.0f);
    float objectProbeUpperHeightWorld = std::clamp(
            map.lightmapSettings.objectProbeUpperHeightWorld, 0.0f, 16.0f);
    if (objectProbeLowerHeightWorld > objectProbeUpperHeightWorld) {
        std::swap(objectProbeLowerHeightWorld, objectProbeUpperHeightWorld);
    }
    std::vector<SectorBakedObjectLightProbe> objectProbes;
    std::vector<SectorBakedObjectLightProbePlacementDiagnostic> objectProbeDiagnostics;
    const auto objectProbeBakeStart = Clock::now();
    if (!BuildSectorBakedObjectLightProbePlacements(
                map,
                SectorBakedObjectLightProbePlacementSettings{
                        objectProbeSpacingWorld,
                        objectProbeLowerHeightWorld,
                        objectProbeUpperHeightWorld},
                objectProbes,
                &objectProbeDiagnostics,
                outError)) {
        if (outError.empty()) {
            outError = "Bake failed: could not place object light probes";
        } else {
            outError = "Bake failed: " + outError;
        }
        removeAtlasOutputs();
        return false;
    }
    if (CheckBakeCancelled(callbacks, outError)) {
        removeAtlasOutputs();
        return false;
    }
    BakeObjectProbeAmbientCubesInScene(
            map,
            bvh,
            triangles,
            alphaOccluders,
            objectProbes);
    const auto objectProbeBakeEnd = Clock::now();
    if (CheckBakeCancelled(callbacks, outError)) {
        removeAtlasOutputs();
        return false;
    }

    const std::string objectProbeSidecarPath = MakeSectorObjectProbeSidecarPathForLightmapPath(outputPath);
    const auto objectProbeSidecarStart = Clock::now();
    if (!WriteSectorBakedObjectLightProbeSidecar(
                objectProbeSidecarPath,
                objectProbes,
                objectProbeSpacingWorld,
                objectProbeLowerHeightWorld,
                objectProbeUpperHeightWorld,
                artifactSourceHash,
                outError)) {
        if (outError.empty()) {
            outError = "Bake failed: could not write object light probe sidecar";
        } else {
            outError = "Bake failed: " + outError;
        }
        removeAtlasOutputs();
        RemoveFileIfExists(objectProbeSidecarPath);
        return false;
    }
    SectorBakedObjectLightProbeMetadata verifiedProbeMetadata;
    std::vector<SectorBakedObjectLightProbe> verifiedProbes;
    SectorBakedObjectLightProbeMetadata expectedProbeMetadata;
    expectedProbeMetadata.version = kSectorBakedObjectLightProbeSidecarVersion;
    expectedProbeMetadata.sourceHash = artifactSourceHash;
    expectedProbeMetadata.count = static_cast<int>(objectProbes.size());
    expectedProbeMetadata.probeSpacingWorld = objectProbeSpacingWorld;
    expectedProbeMetadata.probeLowerHeightWorld = objectProbeLowerHeightWorld;
    expectedProbeMetadata.probeUpperHeightWorld = objectProbeUpperHeightWorld;
    expectedProbeMetadata.format = kSectorBakedObjectLightProbeSidecarFormat;
    if (!ReadSectorBakedObjectLightProbeSidecar(
                objectProbeSidecarPath,
                &expectedProbeMetadata,
                verifiedProbes,
                verifiedProbeMetadata,
                outError)) {
        outError = "Bake failed: stored object probe verification failed: " + outError;
        removeAtlasOutputs();
        RemoveFileIfExists(objectProbeSidecarPath);
        return false;
    }
    const auto objectProbeSidecarEnd = Clock::now();

    std::string staticModelSidecarPath;
    if (staticModels != nullptr && !staticModels->objects.empty()) {
        staticModelSidecarPath =
                MakeSectorStaticModelSidecarPathForLightmapPath(outputPath);
        staticModels->sourceHash = artifactSourceHash;
        if (!WriteSectorStaticModelLightmapSidecar(
                    staticModelSidecarPath,
                    *staticModels,
                    outError)) {
            if (outError.empty()) {
                outError = "Bake failed: could not write static model lightmap sidecar";
            } else {
                outError = "Bake failed: " + outError;
            }
            removeAtlasOutputs();
            RemoveFileIfExists(objectProbeSidecarPath);
            RemoveFileIfExists(staticModelSidecarPath);
            return false;
        }
    }

    outResult.width = width;
    outResult.height = height;
    outResult.sourceHash = artifactSourceHash;
    outResult.artifactVersion = kSectorLightmapArtifactVersion;
    outResult.artifactFormat = kSectorLightmapArtifactFormat;
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
    outResult.staticLightCount = static_cast<int>(
            map.staticLights.size() + map.staticSpotLights.size() + map.staticRectLights.size());
    outResult.staticSpotLightCount = static_cast<int>(map.staticSpotLights.size());
    outResult.staticRectLightCount = static_cast<int>(map.staticRectLights.size());
    outResult.directShadowRays = static_cast<long long>(stats.directHardShadow.raysCast);
    outResult.softShadowSourceRays = static_cast<long long>(stats.softShadowSource.raysCast);
    outResult.ambientOcclusionRays = static_cast<long long>(stats.ambientOcclusion.raysCast);
    outResult.indirectBounceRays = static_cast<long long>(stats.indirectBounce.raysCast);
    outResult.directHardShadowStats = stats.directHardShadow;
    outResult.softShadowSourceStats = stats.softShadowSource;
    outResult.ambientOcclusionStats = stats.ambientOcclusion;
    outResult.indirectBounceStats = stats.indirectBounce;
    outResult.objectProbes.path = objectProbeSidecarPath;
    outResult.objectProbes.version = kSectorBakedObjectLightProbeSidecarVersion;
    outResult.objectProbes.sourceHash = outResult.sourceHash;
    outResult.objectProbes.count = static_cast<int>(objectProbes.size());
    outResult.objectProbes.probeSpacingWorld = objectProbeSpacingWorld;
    outResult.objectProbes.probeLowerHeightWorld = objectProbeLowerHeightWorld;
    outResult.objectProbes.probeUpperHeightWorld = objectProbeUpperHeightWorld;
    outResult.objectProbes.format = kSectorBakedObjectLightProbeSidecarFormat;
    outResult.objectProbes.storedStatistics =
            verifiedProbeMetadata.storedStatistics;
    if (staticModels != nullptr && !staticModels->objects.empty()) {
        outResult.staticModels.path = staticModelSidecarPath;
        outResult.staticModels.version =
                kSectorStaticModelLightmapSidecarVersion;
        outResult.staticModels.sourceHash = outResult.sourceHash;
        outResult.staticModels.modelCount =
                static_cast<int>(staticModels->models.size());
        outResult.staticModels.objectCount =
                static_cast<int>(staticModels->objects.size());
        outResult.staticModels.format =
                kSectorStaticModelLightmapSidecarFormat;
    }
    outResult.objectProbePlacementDiagnostics = static_cast<int>(objectProbeDiagnostics.size());
    outResult.bvhBuildSeconds = std::chrono::duration<double>(bvhBuildEnd - bvhBuildStart).count();
    outResult.directLightingSeconds = std::chrono::duration<double>(directEnd - directStart).count();
    outResult.ambientOcclusionSeconds = std::chrono::duration<double>(aoEnd - aoStart).count();
    outResult.indirectBounceSeconds = std::chrono::duration<double>(indirectEnd - indirectStart).count();
    outResult.objectProbeBakeSeconds = std::chrono::duration<double>(objectProbeBakeEnd - objectProbeBakeStart).count();
    outResult.objectProbeSidecarWriteSeconds =
            std::chrono::duration<double>(objectProbeSidecarEnd - objectProbeSidecarStart).count();
    outResult.gutterExportSeconds = std::chrono::duration<double>(exportEnd - exportStart).count();
    outResult.totalBakeSeconds = std::chrono::duration<double>(objectProbeSidecarEnd - totalStart).count();
    return true;
}

bool BakeSectorLightmap(
        const SectorTopologyMap& map,
        const SectorLightmapLayout& layout,
        const char* outputPath,
        const SectorLightmapBakeCallbacks& callbacks,
        SectorLightmapBakeResult& outResult,
        std::string& outError)
{
    if (HasAssignedSectorStaticModels(map)) {
        outError = "Bake failed: assigned static models were not prepared on the main thread";
        return false;
    }
    return BakeSectorLightmapForMap(
            map,
            layout,
            nullptr,
            outputPath,
            callbacks,
            outResult,
            outError);
}

bool BakeSectorLightmap(
        const SectorTopologyLightmapBakeInput& input,
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
    SectorStaticModelLightmapData staticModels = input.staticModels;
    const bool hasAssignedStaticModels =
            HasAssignedSectorStaticModels(input.mapSnapshot);
    if (hasAssignedStaticModels && staticModels.objects.empty()) {
        outError = "Bake failed: assigned static models were not prepared on the main thread";
        return false;
    }
    if (!staticModels.objects.empty()
            && !PackSectorStaticModelLightmapCharts(
                    staticModels,
                    layout.atlasWidth,
                    layout.atlasHeight,
                    layout.gutter,
                    StaticModelPackCursorAfterTopology(layout),
                    outError)) {
        return false;
    }
    ReportProgress(callbacks, SectorLightmapBakePhase::BuildingLayout, 1, 1);
    if (CheckBakeCancelled(callbacks, outError)) {
        return false;
    }

    const auto layoutEnd = Clock::now();
    if (!BakeSectorLightmapForMap(
                input.mapSnapshot,
                layout,
                staticModels.objects.empty() ? nullptr : &staticModels,
                input.temporaryOutputPath.c_str(),
                callbacks,
                outResult,
                outError)) {
        return false;
    }

    outResult.layoutSeconds = std::chrono::duration<double>(layoutEnd - layoutStart).count();
    outResult.totalBakeSeconds += outResult.layoutSeconds;
    if (!input.expectedSourceHash.empty()) {
        outResult.sourceHash = input.expectedSourceHash;
        outResult.objectProbes.sourceHash = input.expectedSourceHash;
        if (!outResult.staticModels.path.empty()) {
            outResult.staticModels.sourceHash = input.expectedSourceHash;
        }
    }
    return true;
}

std::string ComputeSectorLightmapSourceHash(const SectorTopologyMap& map)
{
    uint64_t hash = 14695981039346656037ull;
    FnvAppendString(hash, "sector-topology-lightmap");
    FnvAppendLightmapBakeConstantsAndSettings(hash, map.lightmapSettings);
    FnvAppendDirectionalLightSettings(hash, map.directionalLight);
    FnvAppendInt(hash, SectorCoordSubdivisions);

    const std::vector<std::string> materialIds = SortedReferencedLightmapTextureIds(map);
    FnvAppendInt(hash, static_cast<int>(materialIds.size()));
    for (const std::string& materialId : materialIds) {
        const SectorMaterialDefinition& texture = map.resolvedMaterialsById.at(materialId);
        FnvAppendString(hash, materialId);
        FnvAppendString(hash, texture.id);
        FnvAppendString(hash, texture.path);
        FnvAppendInt(hash, static_cast<int>(texture.filter));
    }

    const std::vector<const SectorTopologyVertex*> vertices = SortedLightmapHashRecords(map.vertices);
    FnvAppendInt(hash, static_cast<int>(vertices.size()));
    for (const SectorTopologyVertex* vertex : vertices) {
        FnvAppendInt(hash, vertex->id);
        FnvAppendInt(hash, static_cast<int>(vertex->x));
        FnvAppendInt(hash, static_cast<int>(vertex->y));
    }

    const std::vector<const SectorTopologyLineDef*> lineDefs = SortedLightmapHashRecords(map.lineDefs);
    FnvAppendInt(hash, static_cast<int>(lineDefs.size()));
    for (const SectorTopologyLineDef* lineDef : lineDefs) {
        FnvAppendInt(hash, lineDef->id);
        FnvAppendInt(hash, lineDef->startVertexId);
        FnvAppendInt(hash, lineDef->endVertexId);
        FnvAppendInt(hash, lineDef->frontSideDefId);
        FnvAppendInt(hash, lineDef->backSideDefId);
    }

    const std::vector<const SectorTopologySideDef*> sideDefs = SortedLightmapHashRecords(map.sideDefs);
    FnvAppendInt(hash, static_cast<int>(sideDefs.size()));
    for (const SectorTopologySideDef* sideDef : sideDefs) {
        FnvAppendInt(hash, sideDef->id);
        FnvAppendInt(hash, sideDef->lineDefId);
        FnvAppendInt(hash, static_cast<int>(sideDef->side));
        FnvAppendInt(hash, sideDef->sectorId);
        FnvAppendTopologyWallPart(hash, sideDef->wall);
        FnvAppendTopologyWallPart(hash, sideDef->lower);
        FnvAppendTopologyWallPart(hash, sideDef->upper);
        FnvAppendTopologyWallPart(hash, sideDef->middle);
    }

    const std::vector<const SectorTopologySector*> sectors = SortedLightmapHashRecords(map.sectors);
    FnvAppendInt(hash, static_cast<int>(sectors.size()));
    for (const SectorTopologySector* sector : sectors) {
        FnvAppendInt(hash, sector->id);
        FnvAppendFloat(hash, SectorAuthoringToWorldDistance(sector->floorZ));
        FnvAppendFloat(hash, SectorAuthoringToWorldDistance(sector->ceilingZ));
        FnvAppendInt(hash, sector->ceilingSky ? 1 : 0);
        FnvAppendString(hash, sector->floorMaterialId);
        FnvAppendString(hash, sector->ceilingMaterialId);
        FnvAppendTopologyUv(hash, sector->floorUv);
        FnvAppendTopologyUv(hash, sector->ceilingUv);
        FnvAppendColor(hash, sector->ambientColor);
        FnvAppendFloat(hash, sector->ambientIntensity);
        FnvAppendTopologyWallPart(hash, sector->defaultWall);
        FnvAppendTopologyWallPart(hash, sector->defaultLower);
        FnvAppendTopologyWallPart(hash, sector->defaultUpper);
    }

    if (!map.compiledStructuralPrimitives.empty()) {
        FnvAppendString(hash, "structural-primitives-v1");
        std::vector<const SectorCompiledStructuralPrimitive*> primitives;
        primitives.reserve(map.compiledStructuralPrimitives.size());
        for (const SectorCompiledStructuralPrimitive& primitive
                : map.compiledStructuralPrimitives) {
            primitives.push_back(&primitive);
        }
        std::sort(primitives.begin(), primitives.end(), [](const auto* left, const auto* right) {
            return left->sourceAuthoringPrimitiveId < right->sourceAuthoringPrimitiveId;
        });
        FnvAppendInt(hash, static_cast<int>(primitives.size()));
        for (const SectorCompiledStructuralPrimitive* compiled : primitives) {
            const SectorAuthoringStructuralPrimitive& primitive = compiled->authored;
            FnvAppendInt(hash, primitive.id);
            FnvAppendInt(hash, static_cast<int>(primitive.kind));
            FnvAppendInt(hash, primitive.enabled ? 1 : 0);
            FnvAppendInt(hash, primitive.x);
            FnvAppendInt(hash, primitive.z);
            FnvAppendFloat(hash, primitive.yawDegrees);
            FnvAppendInt(hash, primitive.collision ? 1 : 0);
            FnvAppendInt(hash, primitive.receivesLightmap ? 1 : 0);
            FnvAppendInt(hash, primitive.castsBakedShadow ? 1 : 0);
            FnvAppendInt(hash, primitive.castsDynamicShadow ? 1 : 0);
            FnvAppendString(hash, primitive.materials.defaultSurface.materialId);
            FnvAppendTopologyUv(hash, primitive.materials.defaultSurface.uv);
            for (const SectorStructuralMaterialOverride& override
                    : primitive.materials.overrides) {
                FnvAppendInt(hash, override.enabled ? 1 : 0);
                if (override.enabled) {
                    FnvAppendString(hash, override.settings.materialId);
                    FnvAppendTopologyUv(hash, override.settings.uv);
                }
            }
            switch (primitive.kind) {
                case SectorStructuralPrimitiveKind::Box:
                    FnvAppendInt(hash, primitive.box.width); FnvAppendInt(hash, primitive.box.depth);
                    FnvAppendFloat(hash, primitive.box.bottom); FnvAppendFloat(hash, primitive.box.top);
                    break;
                case SectorStructuralPrimitiveKind::Ramp:
                    FnvAppendInt(hash, primitive.ramp.width); FnvAppendInt(hash, primitive.ramp.run);
                    FnvAppendFloat(hash, primitive.ramp.solidBottom); FnvAppendFloat(hash, primitive.ramp.low);
                    FnvAppendFloat(hash, primitive.ramp.high);
                    break;
                case SectorStructuralPrimitiveKind::Stairs:
                    FnvAppendInt(hash, primitive.stairs.width); FnvAppendInt(hash, primitive.stairs.run);
                    FnvAppendFloat(hash, primitive.stairs.bottom); FnvAppendFloat(hash, primitive.stairs.rise);
                    FnvAppendInt(hash, primitive.stairs.stepCount);
                    break;
                case SectorStructuralPrimitiveKind::Cylinder:
                    FnvAppendInt(hash, primitive.cylinder.radius); FnvAppendFloat(hash, primitive.cylinder.bottom);
                    FnvAppendFloat(hash, primitive.cylinder.top); FnvAppendInt(hash, primitive.cylinder.radialSegments);
                    break;
                case SectorStructuralPrimitiveKind::Sphere:
                    FnvAppendInt(hash, primitive.sphere.radius); FnvAppendFloat(hash, primitive.sphere.centerHeight);
                    FnvAppendInt(hash, primitive.sphere.latitudeSegments);
                    FnvAppendInt(hash, primitive.sphere.longitudeSegments);
                    break;
            }
            FnvAppendString(hash, compiled->geometryFingerprint);
        }
    }

    std::vector<const SectorPlacedRuntimeObject*> staticModels;
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind == "static_model"
                && !object.staticModel.modelPath.empty()) {
            staticModels.push_back(&object);
        }
    }
    std::sort(
            staticModels.begin(),
            staticModels.end(),
            [](const auto* left, const auto* right) {
                return left->id < right->id;
            });
    if (!staticModels.empty()) {
        FnvAppendString(hash, "static-models");
        FnvAppendInt(hash, static_cast<int>(staticModels.size()));
        for (const SectorPlacedRuntimeObject* object : staticModels) {
            FnvAppendInt(hash, object->id);
            FnvAppendString(hash, object->staticModel.modelPath);
            FnvAppendVector3(hash, object->position);
            FnvAppendFloat(hash, object->yawRadians);
            if (object->staticModel.rotationXRadians != 0.0f
                    || object->staticModel.rotationZRadians != 0.0f) {
                FnvAppendString(hash, "static-model-rotation-xz");
                FnvAppendFloat(
                        hash,
                        object->staticModel.rotationXRadians);
                FnvAppendFloat(
                        hash,
                        object->staticModel.rotationZRadians);
            }
            FnvAppendFloat(hash, object->staticModel.heightOffsetWorld);
            FnvAppendFloat(hash, object->staticModel.scale);
            if (!object->staticModel.castsShadow) {
                FnvAppendString(hash, "static-model-no-shadow");
            }
            FnvAppendString(
                    hash,
                    object->staticModel.geometryFingerprint);
        }
    }

    const std::vector<const SectorTopologyStaticPointLight*> lights = SortedLightmapHashRecords(map.staticLights);
    FnvAppendInt(hash, static_cast<int>(lights.size()));
    for (const SectorTopologyStaticPointLight* light : lights) {
        const LightmapWorldPointLight worldLight = MakeWorldSpaceLight(*light);
        FnvAppendInt(hash, light->id);
        FnvAppendVector3(hash, worldLight.position);
        FnvAppendColor(hash, light->color);
        FnvAppendFloat(hash, light->intensity);
        FnvAppendFloat(hash, worldLight.radius);
        FnvAppendFloat(hash, std::min(std::clamp(worldLight.sourceRadius, 0.0f, 8.0f), worldLight.radius * 0.5f));
        if (!light->castsShadow) {
            FnvAppendString(hash, "no-shadow");
        }
    }

    if (!map.staticSpotLights.empty()) {
        const std::vector<const SectorTopologyStaticSpotLight*> spotLights =
                SortedLightmapHashRecords(map.staticSpotLights);
        FnvAppendString(hash, "static-spot-lights");
        FnvAppendInt(hash, static_cast<int>(spotLights.size()));
        for (const SectorTopologyStaticSpotLight* light : spotLights) {
            const LightmapWorldSpotLight worldLight = MakeWorldSpaceLight(*light);
            FnvAppendInt(hash, light->id);
            FnvAppendVector3(hash, worldLight.position);
            FnvAppendVector3(hash, worldLight.target);
            FnvAppendColor(hash, light->color);
            FnvAppendFloat(hash, light->intensity);
            FnvAppendFloat(hash, worldLight.range);
            FnvAppendFloat(hash, std::min(std::clamp(worldLight.sourceRadius, 0.0f, 8.0f), worldLight.range * 0.5f));
            FnvAppendFloat(hash, worldLight.innerConeDegrees);
            FnvAppendFloat(hash, worldLight.outerConeDegrees);
            if (!light->castsShadow) {
                FnvAppendString(hash, "no-shadow");
            }
        }
    }

    if (!map.staticRectLights.empty()) {
        const std::vector<const SectorTopologyStaticRectLight*> rectLights =
                SortedLightmapHashRecords(map.staticRectLights);
        FnvAppendString(hash, "static-rect-lights-v1");
        FnvAppendInt(hash, static_cast<int>(rectLights.size()));
        for (const SectorTopologyStaticRectLight* light : rectLights) {
            const LightmapWorldRectLight worldLight = MakeWorldSpaceLight(*light);
            FnvAppendInt(hash, light->id);
            FnvAppendVector3(hash, worldLight.position);
            FnvAppendVector3(hash, worldLight.basis.forward);
            FnvAppendVector3(hash, worldLight.basis.right);
            FnvAppendColor(hash, light->color);
            FnvAppendFloat(hash, light->intensity);
            FnvAppendFloat(hash, worldLight.range);
            FnvAppendFloat(hash, worldLight.width);
            FnvAppendFloat(hash, worldLight.height);
            if (worldLight.startFeather > BakeEpsilon) {
                FnvAppendString(hash, "start-feather");
                FnvAppendFloat(hash, worldLight.startFeather);
            }
            if (!light->castsShadow) FnvAppendString(hash, "no-shadow");
        }
    }

    return HashToString(hash);
}

SectorLightmapStatus GetSectorLightmapStatus(const SectorTopologyMap& map)
{
    return GetSectorLightmapStatus(
            map,
            ComputeSectorLightmapSourceHash(map));
}

SectorLightmapStatus GetSectorLightmapStatus(
        const SectorTopologyMap& map,
        const std::string& currentSourceHash)
{
    if (map.bakedLightmap.path.empty()
            || map.bakedLightmap.width <= 0
            || map.bakedLightmap.height <= 0
            || map.bakedLightmap.sourceHash.empty()) {
        return SectorLightmapStatus::None;
    }

    if (map.bakedLightmap.sourceHash != currentSourceHash) {
        return SectorLightmapStatus::Stale;
    }
    if (map.bakedLightmap.version != kSectorLightmapArtifactVersion
            || map.bakedLightmap.format != kSectorLightmapArtifactFormat) {
        return SectorLightmapStatus::Stale;
    }

    const auto atlasIsInvalid = [&map](const SectorLightmapAtlasMetadata& atlas) {
        return atlas.path.empty() || atlas.width <= 0 || atlas.height <= 0
                || atlas.width != map.bakedLightmap.width
                || atlas.height != map.bakedLightmap.height;
    };
    const SectorLightmapAtlasMetadata primary{
            map.bakedLightmap.path,
            map.bakedLightmap.width,
            map.bakedLightmap.height};
    if (atlasIsInvalid(primary)) {
        return SectorLightmapStatus::Stale;
    }
    if (!FileExistsResolved(ResolveSectorAssetPath(primary.path))) {
        return SectorLightmapStatus::Missing;
    }
    for (const SectorLightmapAtlasMetadata& atlas :
            map.bakedLightmap.additionalAtlases) {
        if (atlasIsInvalid(atlas)) {
            return SectorLightmapStatus::Stale;
        }
        if (!FileExistsResolved(ResolveSectorAssetPath(atlas.path))) {
            return SectorLightmapStatus::Missing;
        }
    }
    return SectorLightmapStatus::Valid;
}

SectorLightmapStatus GetSectorBakedObjectLightProbeStatus(const SectorTopologyMap& map)
{
    return GetSectorBakedObjectLightProbeStatus(
            map, ComputeSectorLightmapSourceHash(map));
}

SectorLightmapStatus GetSectorBakedObjectLightProbeStatus(
        const SectorTopologyMap& map,
        const std::string& currentSourceHash)
{
    const SectorBakedObjectLightProbeMetadata& metadata = map.bakedLightmap.objectProbes;
    if (metadata.path.empty()) {
        return SectorLightmapStatus::None;
    }

    if (metadata.version != kSectorBakedObjectLightProbeSidecarVersion
            || metadata.sourceHash.empty()
            || metadata.count < 0
            || metadata.probeSpacingWorld <= 0.0f
            || metadata.probeLowerHeightWorld < 0.0f
            || metadata.probeUpperHeightWorld
                    < metadata.probeLowerHeightWorld
            || metadata.format != kSectorBakedObjectLightProbeSidecarFormat) {
        return SectorLightmapStatus::Stale;
    }

    if (metadata.sourceHash != currentSourceHash) {
        return SectorLightmapStatus::Stale;
    }

    if (!FileExistsResolved(ResolveSectorAssetPath(metadata.path))) {
        return SectorLightmapStatus::Missing;
    }

    return SectorLightmapStatus::Valid;
}

const char* SectorLightmapStatusText(SectorLightmapStatus status)
{
    switch (status) {
        case SectorLightmapStatus::None: return "No baked lightmap";
        case SectorLightmapStatus::Missing: return "Lightmap missing - rebake required";
        case SectorLightmapStatus::Valid: return "Lightmap valid";
        case SectorLightmapStatus::Stale: return "Lightmap stale - rebake required";
        case SectorLightmapStatus::Invalid: return "Lightmap invalid - rebake required";
    }
    return "No baked lightmap";
}

} // namespace game
