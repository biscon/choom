#include "sector_demo/SectorLightmap.h"

#include "sector_demo/SectorGeneratedGeometry.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace game {

namespace {

struct BakeTriangle {
    Vector3 a = {};
    Vector3 b = {};
    Vector3 c = {};
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
};

constexpr float BakeEpsilon = 0.0001f;
constexpr float RayOriginEpsilon = 0.01f;
constexpr float RayHitEpsilon = 0.001f;
constexpr float Pi = 3.14159265358979323846f;

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
    const Vector3 edge1 = Vector3Subtract(tri.b, tri.a);
    const Vector3 edge2 = Vector3Subtract(tri.c, tri.a);
    const Vector3 h = Vector3CrossProduct(direction, edge2);
    const float det = Vector3DotProduct(edge1, h);
    if (std::fabs(det) <= 0.0000001f) {
        return false;
    }

    const float invDet = 1.0f / det;
    const Vector3 s = Vector3Subtract(origin, tri.a);
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
        return true;
    }
    return false;
}

bool RayIntersectsTriangle(Vector3 origin, Vector3 direction, const BakeTriangle& tri, float maxDistance)
{
    float distance = 0.0f;
    return RayIntersectsTriangle(origin, direction, tri, maxDistance, distance);
}

bool IsOccluded(
        Vector3 position,
        Vector3 normal,
        Vector3 lightPosition,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        const std::vector<BakeTriangle>& triangles)
{
    const Vector3 toLight = Vector3Subtract(lightPosition, position);
    const float distance = Vector3Length(toLight);
    if (distance <= RayHitEpsilon) {
        return false;
    }

    const Vector3 direction = Vector3Scale(toLight, 1.0f / distance);
    const Vector3 origin = Vector3Add(position, Vector3Scale(normal, RayOriginEpsilon));
    const float maxDistance = std::max(0.0f, distance - RayOriginEpsilon * 2.0f);

    for (const BakeTriangle& tri : triangles) {
        if (tri.surfaceIndex == sourceSurfaceIndex && tri.triangleIndex == sourceTriangleIndex) {
            continue;
        }
        if (RayIntersectsTriangle(origin, direction, tri, maxDistance)) {
            return true;
        }
    }

    return false;
}

RayHit TraceRay(
        Vector3 origin,
        Vector3 direction,
        float maxDistance,
        int sourceSurfaceIndex,
        int sourceTriangleIndex,
        const std::vector<BakeTriangle>& triangles)
{
    RayHit closest{};
    closest.distance = maxDistance;
    for (const BakeTriangle& tri : triangles) {
        if (tri.surfaceIndex == sourceSurfaceIndex && tri.triangleIndex == sourceTriangleIndex) {
            continue;
        }

        float distance = 0.0f;
        if (RayIntersectsTriangle(origin, direction, tri, maxDistance, distance)
                && (!closest.hit || distance < closest.distance)) {
            closest.hit = true;
            closest.distance = distance;
        }
    }
    return closest;
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
        const std::vector<BakeTriangle>& triangles)
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

    if (IsOccluded(hit.position, hit.normal, lightPosition, surfaceIndex, hit.triangleIndex, triangles)) {
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
        const std::vector<BakeTriangle>& triangles)
{
    if (light.radius <= 0.0f || light.intensity <= 0.0f) {
        return Vector3{};
    }

    const float sourceRadius = std::min(std::clamp(light.sourceRadius, 0.0f, 8.0f), light.radius * 0.5f);
    if (sourceRadius <= BakeEpsilon) {
        return EvaluateDirectLightSample(light, light.position, hit, surfaceIndex, triangles);
    }

    Vector3 direct{};
    for (int i = 0; i < kDirectSoftShadowSampleCount; ++i) {
        const Vector3 sampleOffset = Vector3Scale(FibonacciSphereSample(i, kDirectSoftShadowSampleCount), sourceRadius);
        const Vector3 samplePosition = Vector3Add(light.position, sampleOffset);
        direct = Vector3Add(direct, EvaluateDirectLightSample(light, samplePosition, hit, surfaceIndex, triangles));
    }
    return Vector3Scale(direct, 1.0f / static_cast<float>(kDirectSoftShadowSampleCount));
}

float BakeAmbientOcclusion(
        const RasterHit& hit,
        int surfaceIndex,
        float radius,
        float strength,
        const std::vector<BakeTriangle>& triangles)
{
    if (strength <= 0.0f || radius <= BakeEpsilon) {
        return 1.0f;
    }

    const Vector3 origin = Vector3Add(hit.position, Vector3Scale(hit.normal, RayOriginEpsilon));
    float occlusion = 0.0f;
    for (int i = 0; i < kAmbientOcclusionSampleCount; ++i) {
        const Vector3 direction = CosineHemisphereSample(hit.normal, i, kAmbientOcclusionSampleCount);
        const RayHit rayHit = TraceRay(origin, direction, radius, surfaceIndex, hit.triangleIndex, triangles);
        if (!rayHit.hit) {
            continue;
        }

        occlusion += 1.0f - std::clamp(rayHit.distance / radius, 0.0f, 1.0f);
    }

    const float averageOcclusion = occlusion / static_cast<float>(kAmbientOcclusionSampleCount);
    return std::clamp(1.0f - strength * averageOcclusion, 0.0f, 1.0f);
}

std::vector<BakeTriangle> BuildBakeTriangles(const SectorGeneratedGeometry& geometry)
{
    std::vector<BakeTriangle> triangles;
    for (size_t surfaceIndex = 0; surfaceIndex < geometry.surfaces.size(); ++surfaceIndex) {
        const SectorGeneratedSurface& surface = geometry.surfaces[surfaceIndex];
        for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
            triangles.push_back(BakeTriangle{
                    surface.vertices[i + 0].position,
                    surface.vertices[i + 1].position,
                    surface.vertices[i + 2].position,
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

bool FileExistsResolved(const std::string& path)
{
    std::ifstream file(path);
    return static_cast<bool>(file);
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

    const int width = layout.atlasWidth;
    const int height = layout.atlasHeight;
    std::vector<Color> pixels(static_cast<size_t>(width * height), Color{0, 0, 0, 255});
    std::vector<unsigned char> valid(static_cast<size_t>(width * height), 0);
    const std::vector<BakeTriangle> triangles = BuildBakeTriangles(geometry);
    const float aoRadius = std::clamp(map.lightmapSettings.ambientOcclusionRadius, 0.05f, 16.0f);
    const float aoStrength = std::clamp(map.lightmapSettings.ambientOcclusionStrength, 0.0f, 1.0f);

    for (const SectorLightmapChart& chart : layout.charts) {
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

                Vector3 direct{};
                if (!map.staticLights.empty()) {
                    for (const SectorStaticPointLight& light : map.staticLights) {
                        direct = Vector3Add(direct, EvaluateDirectLight(light, hit, chart.surfaceIndex, triangles));
                    }
                }

                direct.x = std::clamp(direct.x, 0.0f, 1.0f);
                direct.y = std::clamp(direct.y, 0.0f, 1.0f);
                direct.z = std::clamp(direct.z, 0.0f, 1.0f);
                const float aoFactor = BakeAmbientOcclusion(hit, chart.surfaceIndex, aoRadius, aoStrength, triangles);
                const size_t pixelIndex = static_cast<size_t>(y * width + x);
                pixels[pixelIndex] = Color{FloatToByte(direct.x), FloatToByte(direct.y), FloatToByte(direct.z), FloatToByte(aoFactor)};
                valid[pixelIndex] = 1;
            }
        }

        DilateChart(chart, pixels, valid, width);
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

    outResult.width = width;
    outResult.height = height;
    outResult.sourceHash = ComputeSectorLightmapSourceHash(map);
    return true;
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
    FnvAppendFloat(hash, std::clamp(map.lightmapSettings.ambientOcclusionRadius, 0.05f, 16.0f));
    FnvAppendFloat(hash, std::clamp(map.lightmapSettings.ambientOcclusionStrength, 0.0f, 1.0f));

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
