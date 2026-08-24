#include "sector_demo/SectorStaticModelLightmap.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include "sector_demo/SectorUnits.h"
#include "util/json.hpp"

#include <raymath.h>
#include <xatlas.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace game {
namespace {

constexpr char kStaticModelSidecarMagic[4] = {'S', 'M', 'L', 'M'};
constexpr float kUvEpsilon = 0.000001f;
constexpr float kPositionEpsilon = 0.0000001f;

struct FingerprintCacheEntry {
    std::vector<std::filesystem::path> dependencyPaths;
    std::string dependencyKey;
    std::string fingerprint;
};

std::unordered_map<std::string, FingerprintCacheEntry>& FingerprintCache()
{
    static std::unordered_map<std::string, FingerprintCacheEntry> cache;
    return cache;
}

uint64_t FnvByte(uint64_t hash, unsigned char value)
{
    hash ^= static_cast<uint64_t>(value);
    hash *= 1099511628211ull;
    return hash;
}

void FnvBytes(uint64_t& hash, const void* data, size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash = FnvByte(hash, bytes[i]);
    }
}

void FnvString(uint64_t& hash, const std::string& value)
{
    FnvBytes(hash, value.data(), value.size());
    hash = FnvByte(hash, 0xffu);
}

std::string HexHash(uint64_t hash)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(16, '0');
    for (int i = 15; i >= 0; --i) {
        result[static_cast<size_t>(i)] = digits[hash & 0x0fu];
        hash >>= 4u;
    }
    return result;
}

bool ReadFileBytes(
        const std::filesystem::path& path,
        std::vector<unsigned char>& outBytes,
        std::string& outError)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        outError = "could not open " + path.generic_string();
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    if (length < 0
            || static_cast<uint64_t>(length)
                    > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        outError = "invalid file length for " + path.generic_string();
        return false;
    }
    input.seekg(0, std::ios::beg);
    outBytes.resize(static_cast<size_t>(length));
    if (!outBytes.empty()) {
        input.read(
                reinterpret_cast<char*>(outBytes.data()),
                static_cast<std::streamsize>(outBytes.size()));
    }
    if (!input || input.gcount() != static_cast<std::streamsize>(outBytes.size())) {
        outError = "could not read " + path.generic_string();
        outBytes.clear();
        return false;
    }
    return true;
}

std::string NormalizedFilesystemPath(const std::string& path)
{
    std::error_code ec;
    const std::filesystem::path absolute =
            std::filesystem::absolute(std::filesystem::path(path), ec);
    if (ec) {
        return std::filesystem::path(path).lexically_normal().generic_string();
    }
    return absolute.lexically_normal().generic_string();
}

bool AppendDependencyKey(
        const std::filesystem::path& path,
        std::string& key,
        std::string& outError)
{
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        outError = "could not stat " + path.generic_string();
        return false;
    }
    const auto modified = std::filesystem::last_write_time(path, ec);
    if (ec) {
        outError = "could not read modification time for " + path.generic_string();
        return false;
    }
    key += path.lexically_normal().generic_string();
    key += ':';
    key += std::to_string(size);
    key += ':';
    key += std::to_string(modified.time_since_epoch().count());
    key += ';';
    return true;
}

bool ComputeGeometryFileFingerprint(
        const std::string& assetPath,
        std::string& outFingerprint,
        std::string& outError)
{
    outFingerprint.clear();
    outError.clear();
    const std::string resolved = NormalizedFilesystemPath(
            ResolveSectorAssetPath(assetPath));
    const std::filesystem::path modelPath(resolved);

    auto& cache = FingerprintCache();
    const auto cached = cache.find(resolved);
    if (cached != cache.end()) {
        std::string currentDependencyKey;
        bool dependenciesAvailable = true;
        for (const std::filesystem::path& dependency
                : cached->second.dependencyPaths) {
            std::string dependencyError;
            if (!AppendDependencyKey(
                        dependency,
                        currentDependencyKey,
                        dependencyError)) {
                dependenciesAvailable = false;
                break;
            }
        }
        if (dependenciesAvailable
                && currentDependencyKey == cached->second.dependencyKey) {
            outFingerprint = cached->second.fingerprint;
            return true;
        }
    }

    std::vector<std::filesystem::path> dependencyPaths{modelPath};
    std::string dependencyKey;
    if (!AppendDependencyKey(modelPath, dependencyKey, outError)) {
        return false;
    }

    std::vector<unsigned char> primaryBytes;
    if (!ReadFileBytes(modelPath, primaryBytes, outError)) {
        return false;
    }

    const std::string extension = modelPath.extension().string();
    uint64_t hash = 14695981039346656037ull;
    FnvString(hash, "sector-static-model-geometry-v1");
    if (extension == ".gltf" || extension == ".GLTF") {
        nlohmann::ordered_json document;
        try {
            document = nlohmann::ordered_json::parse(
                    primaryBytes.begin(),
                    primaryBytes.end());
        } catch (const std::exception&) {
            outError = "invalid glTF JSON in " + modelPath.generic_string();
            return false;
        }

        // Material/image state is runtime albedo state, not baked geometry.
        document.erase("images");
        document.erase("textures");
        document.erase("samplers");
        document.erase("materials");
        FnvString(hash, document.dump());

        const auto buffersIt = document.find("buffers");
        if (buffersIt != document.end() && buffersIt->is_array()) {
            for (const auto& buffer : *buffersIt) {
                if (!buffer.is_object()) {
                    continue;
                }
                const auto uriIt = buffer.find("uri");
                if (uriIt == buffer.end() || !uriIt->is_string()) {
                    continue;
                }
                const std::string uri = uriIt->get<std::string>();
                if (uri.rfind("data:", 0) == 0) {
                    FnvString(hash, uri);
                    continue;
                }
                const std::filesystem::path bufferPath =
                        (modelPath.parent_path() / std::filesystem::path(uri))
                                .lexically_normal();
                dependencyPaths.push_back(bufferPath);
                if (!AppendDependencyKey(bufferPath, dependencyKey, outError)) {
                    return false;
                }
                std::vector<unsigned char> bufferBytes;
                if (!ReadFileBytes(bufferPath, bufferBytes, outError)) {
                    return false;
                }
                FnvString(hash, uri);
                FnvBytes(hash, bufferBytes.data(), bufferBytes.size());
            }
        }
    } else if (extension == ".glb" || extension == ".GLB") {
        // A GLB's binary chunk owns its referenced geometry buffers.
        FnvBytes(hash, primaryBytes.data(), primaryBytes.size());
    } else {
        outError = "unsupported static model format " + modelPath.generic_string();
        return false;
    }

    outFingerprint = HexHash(hash);
    cache[resolved] = FingerprintCacheEntry{
            std::move(dependencyPaths),
            dependencyKey,
            outFingerprint};
    return true;
}

bool IsFinite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool IsFinite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

float Cross(Vector2 a, Vector2 b, Vector2 c)
{
    return (b.x - a.x) * (c.y - a.y)
            - (b.y - a.y) * (c.x - a.x);
}

float TriangleUvArea(Vector2 a, Vector2 b, Vector2 c)
{
    return std::fabs(Cross(a, b, c)) * 0.5f;
}

float TriangleWorldArea(Vector3 a, Vector3 b, Vector3 c)
{
    return Vector3Length(Vector3CrossProduct(
            Vector3Subtract(b, a),
            Vector3Subtract(c, a))) * 0.5f;
}

Vector2 LineIntersection(Vector2 a, Vector2 b, Vector2 p, Vector2 q)
{
    const Vector2 r{b.x - a.x, b.y - a.y};
    const Vector2 s{q.x - p.x, q.y - p.y};
    const float denominator = r.x * s.y - r.y * s.x;
    if (std::fabs(denominator) <= kUvEpsilon) {
        return b;
    }
    const Vector2 pa{p.x - a.x, p.y - a.y};
    const float t = (pa.x * s.y - pa.y * s.x) / denominator;
    return Vector2{a.x + r.x * t, a.y + r.y * t};
}

std::vector<Vector2> ClipPolygonAgainstEdge(
        const std::vector<Vector2>& polygon,
        Vector2 edgeA,
        Vector2 edgeB,
        float winding)
{
    std::vector<Vector2> result;
    if (polygon.empty()) {
        return result;
    }
    const auto inside = [edgeA, edgeB, winding](Vector2 point) {
        return Cross(edgeA, edgeB, point) * winding >= -kUvEpsilon;
    };
    Vector2 previous = polygon.back();
    bool previousInside = inside(previous);
    for (Vector2 current : polygon) {
        const bool currentInside = inside(current);
        if (currentInside != previousInside) {
            result.push_back(LineIntersection(previous, current, edgeA, edgeB));
        }
        if (currentInside) {
            result.push_back(current);
        }
        previous = current;
        previousInside = currentInside;
    }
    return result;
}

float PolygonArea(const std::vector<Vector2>& polygon)
{
    float twiceArea = 0.0f;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const Vector2 a = polygon[i];
        const Vector2 b = polygon[(i + 1) % polygon.size()];
        twiceArea += a.x * b.y - a.y * b.x;
    }
    return std::fabs(twiceArea) * 0.5f;
}

bool UvTrianglesHaveInteriorOverlap(
        Vector2 a0,
        Vector2 a1,
        Vector2 a2,
        Vector2 b0,
        Vector2 b1,
        Vector2 b2)
{
    std::vector<Vector2> clipped{a0, a1, a2};
    const float winding = Cross(b0, b1, b2) >= 0.0f ? 1.0f : -1.0f;
    clipped = ClipPolygonAgainstEdge(clipped, b0, b1, winding);
    clipped = ClipPolygonAgainstEdge(clipped, b1, b2, winding);
    clipped = ClipPolygonAgainstEdge(clipped, b2, b0, winding);
    return clipped.size() >= 3 && PolygonArea(clipped) > kUvEpsilon;
}

Vector3 TransformNormal(Vector3 normal, Matrix transform)
{
    const Matrix normalMatrix = MatrixTranspose(MatrixInvert(transform));
    const Vector3 transformed{
            normalMatrix.m0 * normal.x
                    + normalMatrix.m4 * normal.y
                    + normalMatrix.m8 * normal.z,
            normalMatrix.m1 * normal.x
                    + normalMatrix.m5 * normal.y
                    + normalMatrix.m9 * normal.z,
            normalMatrix.m2 * normal.x
                    + normalMatrix.m6 * normal.y
                    + normalMatrix.m10 * normal.z};
    return Vector3LengthSqr(transformed) > kPositionEpsilon
            ? Vector3Normalize(transformed)
            : Vector3{};
}

bool BuildMeshIndices(
        const Mesh& mesh,
        std::vector<uint32_t>& outIndices,
        std::string& outError)
{
    if (mesh.vertexCount <= 0 || mesh.triangleCount <= 0) {
        outError = "mesh has no triangles";
        return false;
    }
    const size_t indexCount = static_cast<size_t>(mesh.triangleCount) * 3;
    outIndices.resize(indexCount);
    if (mesh.indices != nullptr) {
        for (size_t i = 0; i < indexCount; ++i) {
            outIndices[i] = mesh.indices[i];
        }
    } else {
        if (indexCount > static_cast<size_t>(mesh.vertexCount)) {
            outError = "unindexed mesh triangle count exceeds its vertex count";
            return false;
        }
        for (size_t i = 0; i < indexCount; ++i) {
            outIndices[i] = static_cast<uint32_t>(i);
        }
    }
    for (uint32_t index : outIndices) {
        if (index >= static_cast<uint32_t>(mesh.vertexCount)) {
            outError = "mesh index is out of range";
            return false;
        }
    }
    return true;
}

bool AuthoredUv2IsUsable(
        const Mesh& mesh,
        const std::vector<uint32_t>& indices,
        const std::vector<Vector3>& positions,
        float& outWorldArea,
        float& outUvArea)
{
    outWorldArea = 0.0f;
    outUvArea = 0.0f;
    if (mesh.texcoords2 == nullptr) {
        return false;
    }
    std::vector<Vector2> uvs(static_cast<size_t>(mesh.vertexCount));
    for (int i = 0; i < mesh.vertexCount; ++i) {
        uvs[static_cast<size_t>(i)] =
                Vector2{mesh.texcoords2[i * 2], mesh.texcoords2[i * 2 + 1]};
        if (!IsFinite(uvs[static_cast<size_t>(i)])) {
            return false;
        }
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint32_t ia = indices[i];
        const uint32_t ib = indices[i + 1];
        const uint32_t ic = indices[i + 2];
        const float worldArea = TriangleWorldArea(
                positions[ia],
                positions[ib],
                positions[ic]);
        if (worldArea <= kPositionEpsilon) {
            continue;
        }
        const float uvArea = TriangleUvArea(uvs[ia], uvs[ib], uvs[ic]);
        if (uvArea <= kUvEpsilon) {
            return false;
        }
        outWorldArea += worldArea;
        outUvArea += uvArea;
    }
    if (outWorldArea <= kPositionEpsilon || outUvArea <= kUvEpsilon) {
        return false;
    }

    struct UvTriangle {
        Vector2 a;
        Vector2 b;
        Vector2 c;
        float minX = 0.0f;
        float maxX = 0.0f;
        float minY = 0.0f;
        float maxY = 0.0f;
    };
    std::vector<UvTriangle> triangles;
    triangles.reserve(indices.size() / 3);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const Vector2 a = uvs[indices[i]];
        const Vector2 b = uvs[indices[i + 1]];
        const Vector2 c = uvs[indices[i + 2]];
        if (TriangleUvArea(a, b, c) <= kUvEpsilon) {
            continue;
        }
        triangles.push_back(UvTriangle{
                a,
                b,
                c,
                std::min({a.x, b.x, c.x}),
                std::max({a.x, b.x, c.x}),
                std::min({a.y, b.y, c.y}),
                std::max({a.y, b.y, c.y})});
    }
    std::sort(
            triangles.begin(),
            triangles.end(),
            [](const UvTriangle& left, const UvTriangle& right) {
                return left.minX < right.minX;
            });
    for (size_t a = 0; a < triangles.size(); ++a) {
        const UvTriangle& left = triangles[a];
        for (size_t b = a + 1; b < triangles.size(); ++b) {
            const UvTriangle& right = triangles[b];
            if (right.minX >= left.maxX - kUvEpsilon) {
                break;
            }
            if (right.maxY <= left.minY + kUvEpsilon
                    || right.minY >= left.maxY - kUvEpsilon) {
                continue;
            }
            if (UvTrianglesHaveInteriorOverlap(
                        left.a,
                        left.b,
                        left.c,
                        right.a,
                        right.b,
                        right.c)) {
                return false;
            }
        }
    }
    return true;
}

bool PreserveAuthoredUv2(
        const Mesh& mesh,
        const std::vector<uint32_t>& indices,
        const std::vector<Vector3>& positions,
        const std::vector<Vector3>& normals,
        float texelsPerWorldUnit,
        SectorStaticModelLightmapMesh& outMesh)
{
    float worldArea = 0.0f;
    float uvArea = 0.0f;
    if (!AuthoredUv2IsUsable(mesh, indices, positions, worldArea, uvArea)) {
        return false;
    }

    Vector2 minUv{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
    Vector2 maxUv{
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max()};
    std::vector<Vector2> sourceUvs(static_cast<size_t>(mesh.vertexCount));
    for (int i = 0; i < mesh.vertexCount; ++i) {
        const Vector2 uv{mesh.texcoords2[i * 2], mesh.texcoords2[i * 2 + 1]};
        sourceUvs[static_cast<size_t>(i)] = uv;
        minUv.x = std::min(minUv.x, uv.x);
        minUv.y = std::min(minUv.y, uv.y);
        maxUv.x = std::max(maxUv.x, uv.x);
        maxUv.y = std::max(maxUv.y, uv.y);
    }
    const Vector2 extent{maxUv.x - minUv.x, maxUv.y - minUv.y};
    if (extent.x <= kUvEpsilon || extent.y <= kUvEpsilon) {
        return false;
    }

    const float texelScale =
            std::sqrt(worldArea * texelsPerWorldUnit * texelsPerWorldUnit / uvArea);
    if (!std::isfinite(texelScale) || texelScale <= 0.0f) {
        return false;
    }
    outMesh.preservesAuthoredUv2 = true;
    outMesh.originalVertexCount = mesh.vertexCount;
    outMesh.usableWidth =
            std::max(2, static_cast<int>(std::ceil(extent.x * texelScale)));
    outMesh.usableHeight =
            std::max(2, static_cast<int>(std::ceil(extent.y * texelScale)));
    outMesh.sourceVertexIndices.resize(static_cast<size_t>(mesh.vertexCount));
    outMesh.importedPositions = positions;
    outMesh.importedNormals = normals;
    outMesh.localLightmapUvs.resize(static_cast<size_t>(mesh.vertexCount));
    outMesh.indices = indices;
    for (int i = 0; i < mesh.vertexCount; ++i) {
        outMesh.sourceVertexIndices[static_cast<size_t>(i)] =
                static_cast<uint32_t>(i);
        const Vector2 uv = sourceUvs[static_cast<size_t>(i)];
        outMesh.localLightmapUvs[static_cast<size_t>(i)] = Vector2{
                (uv.x - minUv.x) / extent.x,
                (uv.y - minUv.y) / extent.y};
    }
    return true;
}

enum class XatlasUnwrapResult {
    Success,
    NoFiniteChartSet,
    Failure
};

XatlasUnwrapResult UnwrapMeshWithPositions(
        const Mesh& mesh,
        const std::vector<uint32_t>& indices,
        const std::vector<Vector3>& unwrapPositions,
        const std::vector<Vector3>& importedPositions,
        const std::vector<Vector3>& normals,
        float texelsPerWorldUnit,
        SectorStaticModelLightmapMesh& outMesh,
        std::string& outError)
{
    xatlas::Atlas* atlas = xatlas::Create();
    if (atlas == nullptr) {
        outError = "xatlas allocation failed";
        return XatlasUnwrapResult::Failure;
    }

    xatlas::MeshDecl declaration;
    declaration.vertexPositionData = unwrapPositions.data();
    declaration.vertexPositionStride = sizeof(Vector3);
    declaration.vertexNormalData = normals.empty() ? nullptr : normals.data();
    declaration.vertexNormalStride = sizeof(Vector3);
    declaration.vertexCount = static_cast<uint32_t>(unwrapPositions.size());
    declaration.indexData = indices.data();
    declaration.indexCount = static_cast<uint32_t>(indices.size());
    declaration.indexFormat = xatlas::IndexFormat::UInt32;
    const xatlas::AddMeshError addResult =
            xatlas::AddMesh(atlas, declaration, 1);
    if (addResult != xatlas::AddMeshError::Success) {
        outError = std::string("xatlas rejected mesh: ")
                + xatlas::StringForEnum(addResult);
        xatlas::Destroy(atlas);
        return XatlasUnwrapResult::Failure;
    }

    xatlas::ChartOptions chartOptions;
    chartOptions.useInputMeshUvs = false;
    chartOptions.fixWinding = true;
    xatlas::PackOptions packOptions;
    packOptions.padding = 0;
    packOptions.texelsPerUnit = texelsPerWorldUnit;
    packOptions.resolution = 0;
    packOptions.bilinear = false;
    packOptions.blockAlign = false;
    packOptions.bruteForce = false;
    packOptions.createImage = false;
    packOptions.rotateChartsToAxis = true;
    packOptions.rotateCharts = true;
    xatlas::Generate(atlas, chartOptions, packOptions);

    if (atlas->atlasCount != 1
            || atlas->meshCount != 1
            || atlas->width == 0
            || atlas->height == 0
            || atlas->meshes == nullptr) {
        outError = "xatlas could not produce one finite lightmap chart set";
        xatlas::Destroy(atlas);
        return XatlasUnwrapResult::NoFiniteChartSet;
    }
    const xatlas::Mesh& result = atlas->meshes[0];
    if (result.vertexCount == 0
            || result.indexCount == 0
            || result.vertexArray == nullptr
            || result.indexArray == nullptr) {
        outError = "xatlas produced an empty mesh";
        xatlas::Destroy(atlas);
        return XatlasUnwrapResult::NoFiniteChartSet;
    }

    outMesh = {};
    outMesh.originalVertexCount = mesh.vertexCount;
    outMesh.preservesAuthoredUv2 = false;
    outMesh.usableWidth = std::max(2, static_cast<int>(atlas->width));
    outMesh.usableHeight = std::max(2, static_cast<int>(atlas->height));
    outMesh.sourceVertexIndices.resize(result.vertexCount);
    outMesh.importedPositions.resize(result.vertexCount);
    outMesh.importedNormals.resize(result.vertexCount);
    outMesh.localLightmapUvs.resize(result.vertexCount);
    outMesh.indices.assign(
            result.indexArray,
            result.indexArray + result.indexCount);
    for (uint32_t i = 0; i < result.vertexCount; ++i) {
        const xatlas::Vertex& vertex = result.vertexArray[i];
        if (vertex.xref >= importedPositions.size()
                || !std::isfinite(vertex.uv[0])
                || !std::isfinite(vertex.uv[1])) {
            outError = "xatlas produced an invalid vertex remap";
            xatlas::Destroy(atlas);
            return XatlasUnwrapResult::Failure;
        }
        outMesh.sourceVertexIndices[i] = vertex.xref;
        outMesh.importedPositions[i] = importedPositions[vertex.xref];
        outMesh.importedNormals[i] = normals[vertex.xref];
        outMesh.localLightmapUvs[i] = Vector2{
                vertex.uv[0] / static_cast<float>(atlas->width),
                vertex.uv[1] / static_cast<float>(atlas->height)};
    }
    xatlas::Destroy(atlas);
    return XatlasUnwrapResult::Success;
}

bool BuildSmallMeshNormalization(
        const std::vector<Vector3>& positions,
        float texelsPerWorldUnit,
        std::vector<Vector3>& outPositions,
        float& outTexelsPerWorldUnit)
{
    if (positions.empty()) {
        return false;
    }
    Vector3 minPosition = positions.front();
    Vector3 maxPosition = positions.front();
    for (const Vector3 position : positions) {
        minPosition.x = std::min(minPosition.x, position.x);
        minPosition.y = std::min(minPosition.y, position.y);
        minPosition.z = std::min(minPosition.z, position.z);
        maxPosition.x = std::max(maxPosition.x, position.x);
        maxPosition.y = std::max(maxPosition.y, position.y);
        maxPosition.z = std::max(maxPosition.z, position.z);
    }
    const Vector3 extent = Vector3Subtract(maxPosition, minPosition);
    const float largestExtent = std::max({extent.x, extent.y, extent.z});
    constexpr float normalizationTargetExtent = 1.0f;
    if (!std::isfinite(largestExtent)
            || largestExtent <= kPositionEpsilon
            || largestExtent >= normalizationTargetExtent) {
        return false;
    }
    const float scale = normalizationTargetExtent / largestExtent;
    outTexelsPerWorldUnit = texelsPerWorldUnit / scale;
    if (!std::isfinite(scale) || !std::isfinite(outTexelsPerWorldUnit)
            || scale <= 1.0f || outTexelsPerWorldUnit <= 0.0f) {
        return false;
    }

    outPositions.resize(positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
        outPositions[i] = Vector3Scale(
                Vector3Subtract(positions[i], minPosition),
                scale);
    }
    return true;
}

bool UnwrapMesh(
        const Mesh& mesh,
        const std::vector<uint32_t>& indices,
        const std::vector<Vector3>& positions,
        const std::vector<Vector3>& normals,
        float texelsPerWorldUnit,
        SectorStaticModelLightmapMesh& outMesh,
        std::string& outError)
{
    const XatlasUnwrapResult initialResult = UnwrapMeshWithPositions(
            mesh,
            indices,
            positions,
            positions,
            normals,
            texelsPerWorldUnit,
            outMesh,
            outError);
    if (initialResult == XatlasUnwrapResult::Success) {
        return true;
    }
    if (initialResult != XatlasUnwrapResult::NoFiniteChartSet) {
        return false;
    }

    std::vector<Vector3> normalizedPositions;
    float normalizedTexelsPerWorldUnit = 0.0f;
    if (!BuildSmallMeshNormalization(
                positions,
                texelsPerWorldUnit,
                normalizedPositions,
                normalizedTexelsPerWorldUnit)) {
        return false;
    }

    std::string normalizedError;
    const XatlasUnwrapResult normalizedResult = UnwrapMeshWithPositions(
            mesh,
            indices,
            normalizedPositions,
            positions,
            normals,
            normalizedTexelsPerWorldUnit,
            outMesh,
            normalizedError);
    if (normalizedResult == XatlasUnwrapResult::Success) {
        return true;
    }
    outError = "xatlas small-mesh normalization retry failed: "
            + normalizedError;
    return false;
}

Vector3 StaticModelWorldPosition(const SectorPlacedRuntimeObject& object)
{
    Vector3 result = SectorAuthoringToWorldPosition(object.position);
    result.y += object.staticModel.heightOffsetWorld;
    return result;
}

bool WriteU32(std::ostream& output, uint32_t value)
{
    const unsigned char bytes[4] = {
            static_cast<unsigned char>(value & 0xffu),
            static_cast<unsigned char>((value >> 8u) & 0xffu),
            static_cast<unsigned char>((value >> 16u) & 0xffu),
            static_cast<unsigned char>((value >> 24u) & 0xffu)};
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    return output.good();
}

bool WriteI32(std::ostream& output, int32_t value)
{
    return WriteU32(output, static_cast<uint32_t>(value));
}

bool WriteF32(std::ostream& output, float value)
{
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float sidecar field");
    std::memcpy(&bits, &value, sizeof(bits));
    return WriteU32(output, bits);
}

bool WriteString(std::ostream& output, const std::string& value)
{
    if (value.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
        return false;
    }
    if (!WriteU32(output, static_cast<uint32_t>(value.size()))) {
        return false;
    }
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    return output.good();
}

bool ReadU32(std::istream& input, uint32_t& value)
{
    unsigned char bytes[4] = {};
    input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(bytes))) {
        return false;
    }
    value = static_cast<uint32_t>(bytes[0])
            | (static_cast<uint32_t>(bytes[1]) << 8u)
            | (static_cast<uint32_t>(bytes[2]) << 16u)
            | (static_cast<uint32_t>(bytes[3]) << 24u);
    return true;
}

bool ReadI32(std::istream& input, int32_t& value)
{
    uint32_t bits = 0;
    if (!ReadU32(input, bits)) {
        return false;
    }
    value = static_cast<int32_t>(bits);
    return true;
}

bool ReadF32(std::istream& input, float& value)
{
    uint32_t bits = 0;
    if (!ReadU32(input, bits)) {
        return false;
    }
    std::memcpy(&value, &bits, sizeof(value));
    return std::isfinite(value);
}

bool ReadString(std::istream& input, std::string& value, uint32_t maxLength)
{
    uint32_t length = 0;
    if (!ReadU32(input, length) || length > maxLength) {
        return false;
    }
    value.resize(length);
    if (length > 0) {
        input.read(value.data(), static_cast<std::streamsize>(length));
    }
    return input.good()
            || input.gcount() == static_cast<std::streamsize>(length);
}

bool FitsU32(size_t value)
{
    return value <= static_cast<size_t>(std::numeric_limits<uint32_t>::max());
}

} // namespace

bool HasAssignedSectorStaticModels(const SectorTopologyMap& map)
{
    return std::any_of(
            map.runtimeObjects.begin(),
            map.runtimeObjects.end(),
            [](const SectorPlacedRuntimeObject& object) {
                return object.kind == "static_model"
                        && !object.staticModel.modelPath.empty();
            });
}

bool RefreshSectorStaticModelGeometryFingerprints(
        SectorTopologyMap& map,
        std::string& outError)
{
    outError.clear();
    std::unordered_map<std::string, std::string> fingerprints;
    std::vector<SectorPlacedRuntimeObject*> objects;
    for (SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind == "static_model"
                && !object.staticModel.modelPath.empty()) {
            objects.push_back(&object);
        } else if (object.kind == "static_model") {
            object.staticModel.geometryFingerprint.clear();
        }
    }
    std::sort(objects.begin(), objects.end(), [](const auto* left, const auto* right) {
        return left->id < right->id;
    });
    for (SectorPlacedRuntimeObject* object : objects) {
        const std::string normalized =
                std::filesystem::path(object->staticModel.modelPath)
                        .lexically_normal()
                        .generic_string();
        auto cached = fingerprints.find(normalized);
        if (cached == fingerprints.end()) {
            std::string fingerprint;
            std::string error;
            if (!ComputeGeometryFileFingerprint(
                        object->staticModel.modelPath,
                        fingerprint,
                        error)) {
                outError = "Static model object "
                        + std::to_string(object->id)
                        + " ('" + object->staticModel.modelPath
                        + "') fingerprint failed: " + error;
                return false;
            }
            cached = fingerprints.emplace(normalized, std::move(fingerprint)).first;
        }
        object->staticModel.geometryFingerprint = cached->second;
    }
    return true;
}

static bool CopySectorStaticModelForLightmapAtDensity(
        const std::string& modelPath,
        const std::string& geometryFingerprint,
        const Model& model,
        float texelsPerWorldUnit,
        SectorStaticModelLightmapModel& outModel,
        std::string& outError)
{
    outModel = {};
    outError.clear();
    if (model.meshCount <= 0 || model.meshes == nullptr) {
        outError = "model has no meshes";
        return false;
    }
    outModel.modelPath = modelPath;
    outModel.geometryFingerprint = geometryFingerprint;
    outModel.meshes.reserve(static_cast<size_t>(model.meshCount));
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const Mesh& mesh = model.meshes[meshIndex];
        if (mesh.vertices == nullptr) {
            outError = "mesh " + std::to_string(meshIndex)
                    + " has no CPU vertex data";
            return false;
        }
        std::vector<uint32_t> indices;
        if (!BuildMeshIndices(mesh, indices, outError)) {
            outError = "mesh " + std::to_string(meshIndex) + ": " + outError;
            return false;
        }

        std::vector<Vector3> positions(static_cast<size_t>(mesh.vertexCount));
        std::vector<Vector3> normals(static_cast<size_t>(mesh.vertexCount));
        for (int vertexIndex = 0; vertexIndex < mesh.vertexCount; ++vertexIndex) {
            const Vector3 source{
                    mesh.vertices[vertexIndex * 3],
                    mesh.vertices[vertexIndex * 3 + 1],
                    mesh.vertices[vertexIndex * 3 + 2]};
            if (!IsFinite(source)) {
                outError = "mesh " + std::to_string(meshIndex)
                        + " has a non-finite position";
                return false;
            }
            positions[static_cast<size_t>(vertexIndex)] =
                    Vector3Transform(source, model.transform);
            const Vector3 sourceNormal = mesh.normals != nullptr
                    ? Vector3{
                            mesh.normals[vertexIndex * 3],
                            mesh.normals[vertexIndex * 3 + 1],
                            mesh.normals[vertexIndex * 3 + 2]}
                    : Vector3{};
            normals[static_cast<size_t>(vertexIndex)] =
                    IsFinite(sourceNormal)
                    ? TransformNormal(sourceNormal, model.transform)
                    : Vector3{};
        }
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            Vector3& a = normals[indices[i]];
            Vector3& b = normals[indices[i + 1]];
            Vector3& c = normals[indices[i + 2]];
            if (Vector3LengthSqr(a) <= kPositionEpsilon
                    || Vector3LengthSqr(b) <= kPositionEpsilon
                    || Vector3LengthSqr(c) <= kPositionEpsilon) {
                const Vector3 face = Vector3Normalize(Vector3CrossProduct(
                        Vector3Subtract(
                                positions[indices[i + 1]],
                                positions[indices[i]]),
                        Vector3Subtract(
                                positions[indices[i + 2]],
                                positions[indices[i]])));
                if (Vector3LengthSqr(a) <= kPositionEpsilon) {
                    a = face;
                }
                if (Vector3LengthSqr(b) <= kPositionEpsilon) {
                    b = face;
                }
                if (Vector3LengthSqr(c) <= kPositionEpsilon) {
                    c = face;
                }
            }
        }

        SectorStaticModelLightmapMesh prepared;
        if (!PreserveAuthoredUv2(
                    mesh,
                    indices,
                    positions,
                    normals,
                    texelsPerWorldUnit,
                    prepared)
                && !UnwrapMesh(
                        mesh,
                        indices,
                        positions,
                        normals,
                        texelsPerWorldUnit,
                        prepared,
                        outError)) {
            outError = "mesh " + std::to_string(meshIndex)
                    + " unwrap failed: " + outError;
            return false;
        }
        outModel.meshes.push_back(std::move(prepared));
    }
    return true;
}

bool CopySectorStaticModelForLightmap(
        const std::string& modelPath,
        const std::string& geometryFingerprint,
        const Model& model,
        SectorStaticModelLightmapModel& outModel,
        std::string& outError)
{
    return CopySectorStaticModelForLightmapAtDensity(
            modelPath,
            geometryFingerprint,
            model,
            SectorLightmapTexelsPerWorldUnit,
            outModel,
            outError);
}

bool PrepareSectorStaticModelsForLightmapBake(
        SectorTopologyMap& map,
        engine::AssetManager& assets,
        const std::function<bool()>& isCancellationRequested,
        SectorStaticModelLightmapData& outData,
        std::string& outError,
        const SectorStaticModelReadyModelLookup& readyModelLookup)
{
    outData = {};
    outError.clear();
    if (!HasAssignedSectorStaticModels(map)) {
        return true;
    }
    if (isCancellationRequested && isCancellationRequested()) {
        outError = "Bake cancelled";
        return false;
    }
    if (!RefreshSectorStaticModelGeometryFingerprints(map, outError)) {
        return false;
    }
    const float texelsPerWorldUnit = ResolveSectorLightmapBakeQuality(
            map.lightmapSettings.qualityPreset).texelsPerWorldUnit;

    std::vector<SectorPlacedRuntimeObject*> objects;
    for (SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind == "static_model"
                && !object.staticModel.modelPath.empty()) {
            objects.push_back(&object);
        }
    }
    std::sort(objects.begin(), objects.end(), [](const auto* left, const auto* right) {
        return left->id < right->id;
    });

    engine::AssetScopeHandle scope = engine::NullAssetScopeHandle();
    const auto unloadScope = [&assets, &scope]() {
        if (!engine::IsNull(scope)) {
            assets.UnloadScope(scope);
            assets.UpdateMainThread(0.0f);
            scope = engine::NullAssetScopeHandle();
        }
    };

    struct ModelPreparationSource {
        const SectorPlacedRuntimeObject* firstObject = nullptr;
        std::string resolvedPath;
        engine::ModelHandle temporaryHandle = engine::NullModelHandle();
        bool prepared = false;
    };
    std::map<std::string, ModelPreparationSource> sourcesByPath;
    for (const SectorPlacedRuntimeObject* object : objects) {
        if (isCancellationRequested && isCancellationRequested()) {
            unloadScope();
            outError = "Bake cancelled";
            return false;
        }
        const std::string normalized =
                std::filesystem::path(object->staticModel.modelPath)
                        .lexically_normal()
                        .generic_string();
        if (sourcesByPath.find(normalized) != sourcesByPath.end()) {
            continue;
        }
        sourcesByPath.emplace(
                normalized,
                ModelPreparationSource{
                        object,
                        ResolveSectorAssetPath(
                                object->staticModel.modelPath)});
    }

    std::unordered_map<std::string, int> modelIndexByPath;
    outData.models.resize(sourcesByPath.size());
    int modelIndex = 0;
    for (auto& [path, source] : sourcesByPath) {
        if (isCancellationRequested && isCancellationRequested()) {
            outData = {};
            outError = "Bake cancelled";
            return false;
        }
        modelIndexByPath[path] = modelIndex++;
        const SectorPlacedRuntimeObject& object = *source.firstObject;
        const Model* readyModel = readyModelLookup
                ? readyModelLookup(source.resolvedPath)
                : nullptr;
        if (readyModel == nullptr) {
            const engine::ModelHandle readyHandle =
                    assets.FindReadyModelByPath(
                            source.resolvedPath.c_str());
            readyModel = assets.GetModel(readyHandle);
        }
        if (readyModel == nullptr) {
            continue;
        }

        std::string copyError;
        if (!CopySectorStaticModelForLightmapAtDensity(
                    object.staticModel.modelPath,
                    object.staticModel.geometryFingerprint,
                    *readyModel,
                    texelsPerWorldUnit,
                    outData.models[
                            static_cast<size_t>(
                                    modelIndexByPath[path])],
                    copyError)) {
            outData = {};
            outError = "Bake failed: static model object "
                    + std::to_string(object.id)
                    + " ('" + object.staticModel.modelPath
                    + "'): " + copyError;
            return false;
        }
        source.prepared = true;
    }

    const bool needsTemporaryLoads = std::any_of(
            sourcesByPath.begin(),
            sourcesByPath.end(),
            [](const auto& entry) {
                return !entry.second.prepared;
            });
    if (needsTemporaryLoads) {
        scope = assets.CreateScope(
                "sector_static_model_lightmap_bake");
        if (engine::IsNull(scope)) {
            outData = {};
            outError =
                    "Bake failed: could not create static model preparation scope";
            return false;
        }
        for (auto& [path, source] : sourcesByPath) {
            if (source.prepared) {
                continue;
            }
            if (isCancellationRequested && isCancellationRequested()) {
                unloadScope();
                outData = {};
                outError = "Bake cancelled";
                return false;
            }
            source.temporaryHandle = assets.RequestModel(
                    scope,
                    path.c_str(),
                    source.resolvedPath.c_str());
            if (engine::IsNull(source.temporaryHandle)) {
                unloadScope();
                outData = {};
                outError = "Bake failed: static model object "
                        + std::to_string(source.firstObject->id)
                        + " ('"
                        + source.firstObject->staticModel.modelPath
                        + "') could not be requested";
                return false;
            }
        }
        assets.UpdateMainThread(0.0f);
    }

    for (auto& [path, source] : sourcesByPath) {
        if (source.prepared) {
            continue;
        }
        if (isCancellationRequested && isCancellationRequested()) {
            unloadScope();
            outData = {};
            outError = "Bake cancelled";
            return false;
        }
        const SectorPlacedRuntimeObject& object =
                *source.firstObject;
        if (!assets.IsFinished(source.temporaryHandle)
                || assets.HasFailed(source.temporaryHandle)) {
            unloadScope();
            outData = {};
            outError = "Bake failed: static model object "
                    + std::to_string(object.id)
                    + " ('" + object.staticModel.modelPath
                    + "') failed to load";
            return false;
        }
        const Model* model =
                assets.GetModel(source.temporaryHandle);
        if (model == nullptr) {
            unloadScope();
            outData = {};
            outError = "Bake failed: static model object "
                    + std::to_string(object.id)
                    + " ('" + object.staticModel.modelPath
                    + "') has no loaded model data";
            return false;
        }
        std::string copyError;
        if (!CopySectorStaticModelForLightmapAtDensity(
                    object.staticModel.modelPath,
                    object.staticModel.geometryFingerprint,
                    *model,
                    texelsPerWorldUnit,
                    outData.models[
                            static_cast<size_t>(
                                    modelIndexByPath[path])],
                    copyError)) {
            unloadScope();
            outData = {};
            outError = "Bake failed: static model object "
                    + std::to_string(object.id)
                    + " ('" + object.staticModel.modelPath
                    + "'): " + copyError;
            return false;
        }
        source.prepared = true;
    }

    SectorCollisionWorld sectorLookup;
    std::string lookupError;
    if (!sectorLookup.BuildFromTopology(map, &lookupError)) {
        unloadScope();
        outData = {};
        outError = "Bake failed: could not determine static model sectors: "
                + lookupError;
        return false;
    }
    outData.objects.reserve(objects.size());
    for (const SectorPlacedRuntimeObject* object : objects) {
        if (isCancellationRequested && isCancellationRequested()) {
            unloadScope();
            outData = {};
            outError = "Bake cancelled";
            return false;
        }
        const std::string normalized =
                std::filesystem::path(object->staticModel.modelPath)
                        .lexically_normal()
                        .generic_string();
        const auto modelIndex = modelIndexByPath.find(normalized);
        if (modelIndex == modelIndexByPath.end()) {
            unloadScope();
            outData = {};
            outError = "Bake failed: static model object "
                    + std::to_string(object->id)
                    + " model preparation was lost";
            return false;
        }
        const Vector3 worldPosition = StaticModelWorldPosition(*object);
        const int sectorId = sectorLookup.FindSectorContainingPoint(
                Vector2{worldPosition.x, worldPosition.z});
        if (sectorId == 0) {
            unloadScope();
            outData = {};
            outError = "Bake failed: static model object "
                    + std::to_string(object->id)
                    + " ('" + object->staticModel.modelPath
                    + "') is not inside a sector";
            return false;
        }
        SectorStaticModelLightmapObject preparedObject;
        preparedObject.objectId = object->id;
        preparedObject.modelIndex = modelIndex->second;
        preparedObject.containingSectorId = sectorId;
        preparedObject.worldPosition = worldPosition;
        preparedObject.yawRadians = object->yawRadians;
        preparedObject.rotationXRadians =
                object->staticModel.rotationXRadians;
        preparedObject.rotationZRadians =
                object->staticModel.rotationZRadians;
        preparedObject.scale = object->staticModel.scale;
        preparedObject.castsShadow = object->staticModel.castsShadow;
        preparedObject.meshPlacements.resize(
                outData.models[static_cast<size_t>(modelIndex->second)]
                        .meshes.size());
        outData.objects.push_back(std::move(preparedObject));
    }
    unloadScope();
    return true;
}

bool PackSectorStaticModelLightmapCharts(
        SectorStaticModelLightmapData& data,
        int atlasWidth,
        int atlasHeight,
        int gutter,
        SectorStaticModelLightmapPackCursor cursor,
        std::string& outError)
{
    outError.clear();
    for (SectorStaticModelLightmapObject& object : data.objects) {
        if (object.modelIndex < 0
                || object.modelIndex >= static_cast<int>(data.models.size())) {
            outError = "Bake failed: static model object "
                    + std::to_string(object.objectId)
                    + " references invalid prepared model data";
            return false;
        }
        const auto& model = data.models[static_cast<size_t>(object.modelIndex)];
        object.meshPlacements.resize(model.meshes.size());
        for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
            const auto& mesh = model.meshes[meshIndex];
            const int width = mesh.usableWidth + gutter * 2;
            const int height = mesh.usableHeight + gutter * 2;
            if (width > atlasWidth || height > atlasHeight) {
                outError = "Bake failed: static model object "
                        + std::to_string(object.objectId)
                        + " mesh " + std::to_string(meshIndex)
                        + " lightmap chart is larger than the 2048 atlas";
                return false;
            }
            if (cursor.shelfX + width > atlasWidth) {
                cursor.shelfX = 0;
                cursor.shelfY += cursor.shelfHeight;
                cursor.shelfHeight = 0;
            }
            if (cursor.shelfY + height > atlasHeight) {
                ++cursor.atlasIndex;
                cursor.shelfX = 0;
                cursor.shelfY = 0;
                cursor.shelfHeight = 0;
            }

            auto& placement = object.meshPlacements[meshIndex];
            placement.atlasIndex = cursor.atlasIndex;
            placement.x = cursor.shelfX;
            placement.y = cursor.shelfY;
            placement.width = width;
            placement.height = height;
            placement.usableX = cursor.shelfX + gutter;
            placement.usableY = cursor.shelfY + gutter;
            placement.usableWidth = mesh.usableWidth;
            placement.usableHeight = mesh.usableHeight;
            const float minX = static_cast<float>(placement.usableX) + 0.5f;
            const float minY = static_cast<float>(placement.usableY) + 0.5f;
            const float maxX =
                    static_cast<float>(placement.usableX + placement.usableWidth)
                    - 0.5f;
            const float maxY =
                    static_cast<float>(placement.usableY + placement.usableHeight)
                    - 0.5f;
            placement.atlasScale = Vector2{
                    (maxX - minX) / static_cast<float>(atlasWidth),
                    (maxY - minY) / static_cast<float>(atlasHeight)};
            placement.atlasBias = Vector2{
                    minX / static_cast<float>(atlasWidth),
                    minY / static_cast<float>(atlasHeight)};

            cursor.shelfX += width;
            cursor.shelfHeight = std::max(cursor.shelfHeight, height);
        }
    }
    return true;
}

bool WriteSectorStaticModelLightmapSidecar(
        const std::string& path,
        const SectorStaticModelLightmapData& data,
        std::string& outError)
{
    outError.clear();
    if (path.empty()
            || data.sourceHash.empty()
            || !FitsU32(data.models.size())
            || !FitsU32(data.objects.size())) {
        outError = "Static model lightmap sidecar write failed: invalid header";
        return false;
    }
    const std::filesystem::path outputPath(path);
    std::error_code ec;
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path(), ec);
    }
    if (ec) {
        outError = "Static model lightmap sidecar write failed: could not create output directory";
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        outError = "Static model lightmap sidecar write failed: could not open output";
        return false;
    }
    output.write(kStaticModelSidecarMagic, sizeof(kStaticModelSidecarMagic));
    if (!WriteU32(output, kSectorStaticModelLightmapSidecarVersion)
            || !WriteString(output, data.sourceHash)
            || !WriteString(output, kSectorStaticModelLightmapSidecarFormat)
            || !WriteU32(output, static_cast<uint32_t>(data.models.size()))
            || !WriteU32(output, static_cast<uint32_t>(data.objects.size()))) {
        outError = "Static model lightmap sidecar write failed: header write failed";
        return false;
    }
    for (const auto& model : data.models) {
        if (!FitsU32(model.meshes.size())
                || !WriteString(output, model.modelPath)
                || !WriteString(output, model.geometryFingerprint)
                || !WriteU32(output, static_cast<uint32_t>(model.meshes.size()))) {
            outError = "Static model lightmap sidecar write failed: model header write failed";
            return false;
        }
        for (const auto& mesh : model.meshes) {
            const size_t vertexCount = mesh.sourceVertexIndices.size();
            if (vertexCount != mesh.localLightmapUvs.size()
                    || !FitsU32(vertexCount)
                    || !FitsU32(mesh.indices.size())
                    || !WriteU32(output, static_cast<uint32_t>(mesh.originalVertexCount))
                    || !WriteU32(output, mesh.preservesAuthoredUv2 ? 1u : 0u)
                    || !WriteU32(output, static_cast<uint32_t>(vertexCount))
                    || !WriteU32(output, static_cast<uint32_t>(mesh.indices.size()))) {
                outError = "Static model lightmap sidecar write failed: mesh header write failed";
                return false;
            }
            for (size_t i = 0; i < vertexCount; ++i) {
                if (!IsFinite(mesh.localLightmapUvs[i])
                        || !WriteU32(output, mesh.sourceVertexIndices[i])
                        || !WriteF32(output, mesh.localLightmapUvs[i].x)
                        || !WriteF32(output, mesh.localLightmapUvs[i].y)) {
                    outError = "Static model lightmap sidecar write failed: vertex write failed";
                    return false;
                }
            }
            for (uint32_t index : mesh.indices) {
                if (!WriteU32(output, index)) {
                    outError = "Static model lightmap sidecar write failed: index write failed";
                    return false;
                }
            }
        }
    }
    for (const auto& object : data.objects) {
        if (object.modelIndex < 0
                || !FitsU32(object.meshPlacements.size())
                || !WriteI32(output, object.objectId)
                || !WriteU32(output, static_cast<uint32_t>(object.modelIndex))
                || !WriteI32(output, object.containingSectorId)
                || !WriteU32(output, static_cast<uint32_t>(object.meshPlacements.size()))) {
            outError = "Static model lightmap sidecar write failed: object header write failed";
            return false;
        }
        for (const auto& placement : object.meshPlacements) {
            if (placement.atlasIndex < 0
                    || !IsFinite(placement.atlasScale)
                    || !IsFinite(placement.atlasBias)
                    || !WriteU32(output, static_cast<uint32_t>(placement.atlasIndex))
                    || !WriteF32(output, placement.atlasScale.x)
                    || !WriteF32(output, placement.atlasScale.y)
                    || !WriteF32(output, placement.atlasBias.x)
                    || !WriteF32(output, placement.atlasBias.y)) {
                outError = "Static model lightmap sidecar write failed: placement write failed";
                return false;
            }
        }
    }
    if (!output.good()) {
        outError = "Static model lightmap sidecar write failed";
        return false;
    }
    return true;
}

bool ReadSectorStaticModelLightmapSidecar(
        const std::string& path,
        const SectorBakedStaticModelLightmapMetadata* expectedMetadata,
        SectorStaticModelLightmapData& outData,
        std::string& outError)
{
    outData = {};
    outError.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        outError = "Static model lightmap sidecar read failed: could not open input";
        return false;
    }
    char magic[4] = {};
    input.read(magic, sizeof(magic));
    uint32_t version = 0;
    std::string sourceHash;
    std::string format;
    uint32_t modelCount = 0;
    uint32_t objectCount = 0;
    if (input.gcount() != static_cast<std::streamsize>(sizeof(magic))
            || !std::equal(
                    std::begin(magic),
                    std::end(magic),
                    std::begin(kStaticModelSidecarMagic))
            || !ReadU32(input, version)
            || !ReadString(input, sourceHash, 4096)
            || !ReadString(input, format, 256)
            || !ReadU32(input, modelCount)
            || !ReadU32(input, objectCount)
            || version != kSectorStaticModelLightmapSidecarVersion
            || format != kSectorStaticModelLightmapSidecarFormat
            || modelCount > 100000u
            || objectCount > 1000000u) {
        outError = "Static model lightmap sidecar read failed: invalid or truncated header";
        return false;
    }
    if (expectedMetadata != nullptr
            && (expectedMetadata->version != static_cast<int>(version)
                    || expectedMetadata->sourceHash != sourceHash
                    || expectedMetadata->modelCount != static_cast<int>(modelCount)
                    || expectedMetadata->objectCount != static_cast<int>(objectCount)
                    || expectedMetadata->format != format)) {
        outError = "Static model lightmap sidecar read failed: metadata mismatch";
        return false;
    }

    outData.sourceHash = sourceHash;
    outData.models.resize(modelCount);
    for (auto& model : outData.models) {
        uint32_t meshCount = 0;
        if (!ReadString(input, model.modelPath, 1024 * 1024)
                || !ReadString(input, model.geometryFingerprint, 4096)
                || !ReadU32(input, meshCount)
                || meshCount > 100000u) {
            outError = "Static model lightmap sidecar read failed: invalid model header";
            outData = {};
            return false;
        }
        model.meshes.resize(meshCount);
        for (auto& mesh : model.meshes) {
            uint32_t originalVertexCount = 0;
            uint32_t authored = 0;
            uint32_t vertexCount = 0;
            uint32_t indexCount = 0;
            if (!ReadU32(input, originalVertexCount)
                    || !ReadU32(input, authored)
                    || !ReadU32(input, vertexCount)
                    || !ReadU32(input, indexCount)
                    || originalVertexCount > 100000000u
                    || vertexCount > 100000000u
                    || indexCount > 300000000u
                    || indexCount % 3u != 0u) {
                outError = "Static model lightmap sidecar read failed: invalid mesh header";
                outData = {};
                return false;
            }
            mesh.originalVertexCount = static_cast<int>(originalVertexCount);
            mesh.preservesAuthoredUv2 = authored != 0;
            mesh.sourceVertexIndices.resize(vertexCount);
            mesh.localLightmapUvs.resize(vertexCount);
            for (uint32_t i = 0; i < vertexCount; ++i) {
                if (!ReadU32(input, mesh.sourceVertexIndices[i])
                        || mesh.sourceVertexIndices[i] >= originalVertexCount
                        || !ReadF32(input, mesh.localLightmapUvs[i].x)
                        || !ReadF32(input, mesh.localLightmapUvs[i].y)
                        || !IsFinite(mesh.localLightmapUvs[i])) {
                    outError = "Static model lightmap sidecar read failed: invalid vertex remap";
                    outData = {};
                    return false;
                }
            }
            mesh.indices.resize(indexCount);
            for (uint32_t& index : mesh.indices) {
                if (!ReadU32(input, index) || index >= vertexCount) {
                    outError = "Static model lightmap sidecar read failed: invalid mesh index";
                    outData = {};
                    return false;
                }
            }
        }
    }
    outData.objects.resize(objectCount);
    for (auto& object : outData.objects) {
        int32_t objectId = 0;
        uint32_t modelIndex = 0;
        int32_t sectorId = 0;
        uint32_t meshCount = 0;
        if (!ReadI32(input, objectId)
                || !ReadU32(input, modelIndex)
                || !ReadI32(input, sectorId)
                || !ReadU32(input, meshCount)
                || modelIndex >= outData.models.size()
                || meshCount
                        != outData.models[modelIndex].meshes.size()) {
            outError = "Static model lightmap sidecar read failed: invalid object header";
            outData = {};
            return false;
        }
        object.objectId = objectId;
        object.modelIndex = static_cast<int>(modelIndex);
        object.containingSectorId = sectorId;
        object.meshPlacements.resize(meshCount);
        for (auto& placement : object.meshPlacements) {
            uint32_t atlasIndex = 0;
            if (!ReadU32(input, atlasIndex)
                    || atlasIndex > static_cast<uint32_t>(std::numeric_limits<int>::max())
                    || !ReadF32(input, placement.atlasScale.x)
                    || !ReadF32(input, placement.atlasScale.y)
                    || !ReadF32(input, placement.atlasBias.x)
                    || !ReadF32(input, placement.atlasBias.y)
                    || !IsFinite(placement.atlasScale)
                    || !IsFinite(placement.atlasBias)
                    || placement.atlasScale.x < 0.0f
                    || placement.atlasScale.y < 0.0f) {
                outError = "Static model lightmap sidecar read failed: invalid placement";
                outData = {};
                return false;
            }
            placement.atlasIndex = static_cast<int>(atlasIndex);
        }
    }
    char trailing = 0;
    if (input.read(&trailing, 1)) {
        outError = "Static model lightmap sidecar read failed: trailing data";
        outData = {};
        return false;
    }
    if (!input.eof()) {
        outError = "Static model lightmap sidecar read failed: truncated input";
        outData = {};
        return false;
    }
    return true;
}

bool AreSectorStaticModelLightmapAtlasIndicesValid(
        const SectorStaticModelLightmapData& data,
        int atlasCount)
{
    if (atlasCount <= 0) {
        return false;
    }
    for (const SectorStaticModelLightmapObject& object : data.objects) {
        for (const SectorStaticModelLightmapMeshPlacement& placement
                : object.meshPlacements) {
            if (placement.atlasIndex < 0
                    || placement.atlasIndex >= atlasCount) {
                return false;
            }
        }
    }
    return true;
}

SectorLightmapStatus GetSectorStaticModelLightmapStatus(
        const SectorTopologyMap& map)
{
    const bool required = HasAssignedSectorStaticModels(map);
    const auto& metadata = map.bakedLightmap.staticModels;
    if (metadata.path.empty()) {
        return required ? SectorLightmapStatus::Stale : SectorLightmapStatus::None;
    }
    if (metadata.version != kSectorStaticModelLightmapSidecarVersion
            || metadata.sourceHash.empty()
            || metadata.modelCount <= 0
            || metadata.objectCount <= 0
            || metadata.format != kSectorStaticModelLightmapSidecarFormat
            || metadata.sourceHash != ComputeSectorLightmapSourceHash(map)) {
        return SectorLightmapStatus::Stale;
    }
    if (!std::filesystem::exists(ResolveSectorAssetPath(metadata.path))) {
        return SectorLightmapStatus::Missing;
    }
    return SectorLightmapStatus::Valid;
}

std::string MakeSectorStaticModelSidecarPathForLightmapPath(
        const std::string& lightmapPath)
{
    return lightmapPath + ".static_models.bin";
}

} // namespace game
