#include "sector_demo/renderer/SectorPbrEnvironment.h"

#include "engine/assets/AssetManager.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorSkyCylinder.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace game {
namespace {

constexpr int PbrEnvironmentFaceSize = 256;

unsigned char LinearToSrgbByte(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    const float encoded = engine::LinearNormalizedChannelToSrgb(value);
    return static_cast<unsigned char>(std::clamp(encoded * 255.0f + 0.5f, 0.0f, 255.0f));
}

Vector3 CubemapDirection(int face, int x, int y, int size)
{
    const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
    const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;
    switch (face) {
        case 0: return Vector3Normalize(Vector3{1.0f, -v, -u});
        case 1: return Vector3Normalize(Vector3{-1.0f, -v, u});
        case 2: return Vector3Normalize(Vector3{u, 1.0f, v});
        case 3: return Vector3Normalize(Vector3{u, -1.0f, -v});
        case 4: return Vector3Normalize(Vector3{u, -v, 1.0f});
        default: return Vector3Normalize(Vector3{-u, -v, -1.0f});
    }
}

Color SampleSky(
        const Color* colors,
        int width,
        int height,
        Vector3 direction,
        const SectorTopologySkySettings& settings)
{
    const float yaw = settings.yawOffsetDegrees * DEG2RAD;
    const float cosYaw = std::cos(-yaw);
    const float sinYaw = std::sin(-yaw);
    direction = Vector3{
            direction.x * cosYaw - direction.z * sinYaw,
            direction.y,
            direction.x * sinYaw + direction.z * cosYaw};
    float u = std::atan2(direction.z, direction.x) / (2.0f * PI) + 0.5f;
    u -= std::floor(u);
    float v = std::acos(std::clamp(direction.y, -1.0f, 1.0f)) / PI;
    v = v * settings.verticalScale + settings.verticalOffset;
    if (direction.y > 0.985f || v < 0.0f) {
        return settings.topColor;
    }
    v = std::clamp(v, 0.0f, 1.0f);
    const int ix = std::clamp(static_cast<int>(u * static_cast<float>(width)), 0, width - 1);
    const int iy = std::clamp(static_cast<int>(v * static_cast<float>(height)), 0, height - 1);
    return colors[iy * width + ix];
}

Color LinearAverage(Color a, Color b, Color c, Color d)
{
    const auto average = [](unsigned char av, unsigned char bv, unsigned char cv, unsigned char dv) {
        const float linear = (engine::SrgbNormalizedChannelToLinear(static_cast<float>(av) / 255.0f)
                + engine::SrgbNormalizedChannelToLinear(static_cast<float>(bv) / 255.0f)
                + engine::SrgbNormalizedChannelToLinear(static_cast<float>(cv) / 255.0f)
                + engine::SrgbNormalizedChannelToLinear(static_cast<float>(dv) / 255.0f)) * 0.25f;
        return LinearToSrgbByte(linear);
    };
    return Color{
            average(a.r, b.r, c.r, d.r),
            average(a.g, b.g, c.g, d.g),
            average(a.b, b.b, c.b, d.b),
            255};
}

Image BuildCubemapImage(const Image& source, const SectorTopologySkySettings& settings)
{
    const Color* sourceColors = LoadImageColors(source);
    if (sourceColors == nullptr) {
        return Image{};
    }
    int mipCount = 1;
    for (int size = PbrEnvironmentFaceSize; size > 1; size /= 2) {
        ++mipCount;
    }

    size_t pixelCount = 0;
    for (int size = PbrEnvironmentFaceSize, mip = 0; mip < mipCount; ++mip, size = std::max(1, size / 2)) {
        pixelCount += static_cast<size_t>(size) * static_cast<size_t>(size) * 6u;
    }
    std::vector<Color> pixels(pixelCount);
    size_t writeOffset = 0;
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < PbrEnvironmentFaceSize; ++y) {
            for (int x = 0; x < PbrEnvironmentFaceSize; ++x) {
                pixels[writeOffset
                        + static_cast<size_t>(face * PbrEnvironmentFaceSize * PbrEnvironmentFaceSize)
                        + static_cast<size_t>(y * PbrEnvironmentFaceSize + x)] = SampleSky(
                                sourceColors,
                                source.width,
                                source.height,
                                CubemapDirection(face, x, y, PbrEnvironmentFaceSize),
                                settings);
            }
        }
    }
    writeOffset += static_cast<size_t>(PbrEnvironmentFaceSize)
            * static_cast<size_t>(PbrEnvironmentFaceSize) * 6u;
    size_t previousOffset = 0;
    int previousSize = PbrEnvironmentFaceSize;
    for (int mip = 1; mip < mipCount; ++mip) {
        const int size = std::max(1, previousSize / 2);
        for (int face = 0; face < 6; ++face) {
            const size_t previousFace = previousOffset
                    + static_cast<size_t>(face * previousSize * previousSize);
            const size_t faceOffset = writeOffset
                    + static_cast<size_t>(face * size * size);
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const int x0 = std::min(previousSize - 1, x * 2);
                    const int y0 = std::min(previousSize - 1, y * 2);
                    const int x1 = std::min(previousSize - 1, x0 + 1);
                    const int y1 = std::min(previousSize - 1, y0 + 1);
                    pixels[faceOffset + static_cast<size_t>(y * size + x)] = LinearAverage(
                            pixels[previousFace + static_cast<size_t>(y0 * previousSize + x0)],
                            pixels[previousFace + static_cast<size_t>(y0 * previousSize + x1)],
                            pixels[previousFace + static_cast<size_t>(y1 * previousSize + x0)],
                            pixels[previousFace + static_cast<size_t>(y1 * previousSize + x1)]);
                }
            }
        }
        previousOffset = writeOffset;
        previousSize = size;
        writeOffset += static_cast<size_t>(size) * static_cast<size_t>(size) * 6u;
    }
    UnloadImageColors(const_cast<Color*>(sourceColors));

    Image image{};
    image.data = MemAlloc(static_cast<unsigned int>(pixels.size() * sizeof(Color)));
    if (image.data == nullptr) {
        return image;
    }
    std::memcpy(image.data, pixels.data(), pixels.size() * sizeof(Color));
    image.width = PbrEnvironmentFaceSize;
    image.height = PbrEnvironmentFaceSize * 6;
    image.mipmaps = mipCount;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return image;
}

} // namespace

bool BuildSectorPbrEnvironment(
        engine::AssetManager& assets,
        engine::AssetScopeHandle scope,
        const SectorTopologyMap& map,
        SectorPbrEnvironment& outEnvironment)
{
    outEnvironment = {};
    Image source{};
    const SectorMaterialDefinition* skyTexture = FindSkyTexture(map);
    if (ShouldRenderSkyCylinder(map) && skyTexture != nullptr) {
        source = LoadImage(ResolveSectorAssetPath(skyTexture->path).c_str());
        outEnvironment.usedSky = source.data != nullptr;
    }
    if (source.data == nullptr) {
        // No real environment is different from a neutral environment. Keep
        // the handle and eligibility clear so map switches cannot retain a
        // previous sky contribution.
        return true;
    }
    const SectorTopologySkySettings settings = NormalizeSectorTopologySkySettings(map.skySettings);
    Image cubemapImage = BuildCubemapImage(source, settings);
    UnloadImage(source);
    if (cubemapImage.data == nullptr) {
        std::fprintf(stderr, "[SectorMeshRenderer WARNING] Could not build PBR environment; props will use direct lighting only\n");
        return false;
    }
    outEnvironment.cubemap = assets.CreateCubemapFromImage(
            scope,
            "sector_pbr_environment",
            cubemapImage,
            engine::TextureColorUsage::SceneSrgb,
            CUBEMAP_LAYOUT_LINE_VERTICAL);
    UnloadImage(cubemapImage);
    outEnvironment.active = !engine::IsNull(outEnvironment.cubemap);
    return outEnvironment.active;
}

} // namespace game
