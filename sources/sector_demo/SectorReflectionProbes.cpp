#include "sector_demo/SectorReflectionProbes.h"

#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <type_traits>
#include <raymath.h>

namespace game {
namespace {

constexpr std::array<char, 8> Magic{{'S', 'R', 'P', 'R', 'O', 'B', 'E', '\0'}};
constexpr std::uint32_t MaximumProbeCount = 4096;
constexpr std::uint32_t MaximumHashBytes = 1024;

void HashBytes(std::uint64_t& hash, const void* data, std::size_t size)
{
    constexpr std::uint64_t Prime = 1099511628211ull;
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= Prime;
    }
}

template<typename T>
void HashValue(std::uint64_t& hash, const T& value)
{
    static_assert(std::is_trivially_copyable<T>::value, "hash value must be POD");
    HashBytes(hash, &value, sizeof(value));
}

void HashString(std::uint64_t& hash, const std::string& value)
{
    const std::uint64_t size = value.size();
    HashValue(hash, size);
    HashBytes(hash, value.data(), value.size());
}

void HashAssetFile(std::uint64_t& hash, const std::string& assetPath)
{
    const std::string resolved = ResolveSectorAssetPath(assetPath);
    std::ifstream input(resolved, std::ios::binary);
    if (!input) {
        const std::uint8_t missing = 0;
        HashValue(hash, missing);
        return;
    }
    const std::uint8_t present = 1;
    HashValue(hash, present);
    std::array<char, 16384> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) HashBytes(hash, buffer.data(), static_cast<std::size_t>(count));
    }
}

void WriteU32(std::ostream& output, std::uint32_t value)
{
    const unsigned char bytes[4]{
            static_cast<unsigned char>(value & 0xffu),
            static_cast<unsigned char>((value >> 8u) & 0xffu),
            static_cast<unsigned char>((value >> 16u) & 0xffu),
            static_cast<unsigned char>((value >> 24u) & 0xffu)};
    output.write(reinterpret_cast<const char*>(bytes), 4);
}

void WriteU16(std::ostream& output, std::uint16_t value)
{
    const unsigned char bytes[2]{
            static_cast<unsigned char>(value & 0xffu),
            static_cast<unsigned char>((value >> 8u) & 0xffu)};
    output.write(reinterpret_cast<const char*>(bytes), 2);
}

bool ReadU32(std::istream& input, std::uint32_t& value)
{
    unsigned char bytes[4]{};
    input.read(reinterpret_cast<char*>(bytes), 4);
    if (!input) return false;
    value = static_cast<std::uint32_t>(bytes[0])
            | (static_cast<std::uint32_t>(bytes[1]) << 8u)
            | (static_cast<std::uint32_t>(bytes[2]) << 16u)
            | (static_cast<std::uint32_t>(bytes[3]) << 24u);
    return true;
}

bool ReadU16(std::istream& input, std::uint16_t& value)
{
    unsigned char bytes[2]{};
    input.read(reinterpret_cast<char*>(bytes), 2);
    if (!input) return false;
    value = static_cast<std::uint16_t>(bytes[0])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8u);
    return true;
}

bool ValidResolution(int resolution)
{
    return resolution == 64 || resolution == 128 || resolution == 256;
}

Vector3 CubemapDirection(int face, int x, int y, int size)
{
    const float u = 2.0f * (static_cast<float>(x) + 0.5f)
                    / static_cast<float>(size) - 1.0f;
    const float v = 2.0f * (static_cast<float>(y) + 0.5f)
                    / static_cast<float>(size) - 1.0f;
    switch (face) {
        case 0: return Vector3Normalize({1.0f, -v, -u});
        case 1: return Vector3Normalize({-1.0f, -v, u});
        case 2: return Vector3Normalize({u, 1.0f, v});
        case 3: return Vector3Normalize({u, -1.0f, -v});
        case 4: return Vector3Normalize({u, -v, 1.0f});
        default: return Vector3Normalize({-u, -v, -1.0f});
    }
}

Vector4 SampleCapturedCubemap(
        const std::vector<Vector4>& pixels,
        int size,
        Vector3 direction)
{
    direction = Vector3Normalize(direction);
    const float ax = std::fabs(direction.x);
    const float ay = std::fabs(direction.y);
    const float az = std::fabs(direction.z);
    int face = 0;
    float u = 0.0f;
    float v = 0.0f;
    if (ax >= ay && ax >= az) {
        if (direction.x >= 0.0f) {
            face = 0; u = -direction.z / ax; v = -direction.y / ax;
        } else {
            face = 1; u = direction.z / ax; v = -direction.y / ax;
        }
    } else if (ay >= az) {
        if (direction.y >= 0.0f) {
            face = 2; u = direction.x / ay; v = direction.z / ay;
        } else {
            face = 3; u = direction.x / ay; v = -direction.z / ay;
        }
    } else if (direction.z >= 0.0f) {
        face = 4; u = direction.x / az; v = -direction.y / az;
    } else {
        face = 5; u = -direction.x / az; v = -direction.y / az;
    }
    const float fx = std::clamp((u * 0.5f + 0.5f) * size - 0.5f,
            0.0f, static_cast<float>(size - 1));
    const float fy = std::clamp((v * 0.5f + 0.5f) * size - 0.5f,
            0.0f, static_cast<float>(size - 1));
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = std::min(size - 1, x0 + 1);
    const int y1 = std::min(size - 1, y0 + 1);
    const float tx = fx - x0;
    const float ty = fy - y0;
    const std::size_t base = static_cast<std::size_t>(face * size * size);
    const auto at = [&](int x, int y) {
        return pixels[base + static_cast<std::size_t>(y * size + x)];
    };
    const Vector4 a = Vector4Lerp(at(x0, y0), at(x1, y0), tx);
    const Vector4 b = Vector4Lerp(at(x0, y1), at(x1, y1), tx);
    return Vector4Lerp(a, b, ty);
}

float RadicalInverseVdc(std::uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

Vector3 ImportanceSampleGgx(float xiX, float xiY, float roughness, Vector3 normal)
{
    const float a = roughness * roughness;
    const float phi = 2.0f * PI * xiX;
    const float cosTheta = std::sqrt((1.0f - xiY)
            / std::max(1.0f + (a * a - 1.0f) * xiY, 0.00001f));
    const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    const Vector3 halfTangent{
            std::cos(phi) * sinTheta,
            std::sin(phi) * sinTheta,
            cosTheta};
    const Vector3 up = std::fabs(normal.z) < 0.999f
            ? Vector3{0.0f, 0.0f, 1.0f}
            : Vector3{1.0f, 0.0f, 0.0f};
    const Vector3 tangent = Vector3Normalize(Vector3CrossProduct(up, normal));
    const Vector3 bitangent = Vector3CrossProduct(normal, tangent);
    return Vector3Normalize(Vector3Add(
            Vector3Add(Vector3Scale(tangent, halfTangent.x),
                    Vector3Scale(bitangent, halfTangent.y)),
            Vector3Scale(normal, halfTangent.z)));
}

} // namespace

int SectorReflectionProbeMipCount(int resolution)
{
    if (!ValidResolution(resolution)) return 0;
    int count = 1;
    while (resolution > 1) {
        resolution /= 2;
        ++count;
    }
    return count;
}

std::size_t SectorReflectionProbeHalfCount(int resolution, int mipCount)
{
    if (!ValidResolution(resolution)
            || mipCount != SectorReflectionProbeMipCount(resolution)) return 0;
    std::size_t texels = 0;
    for (int mip = 0, size = resolution; mip < mipCount; ++mip, size = std::max(1, size / 2)) {
        texels += static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 6u;
    }
    return texels * 4u;
}

std::string ComputeSectorReflectionProbeSourceHash(
        const SectorTopologyMap& map,
        const SectorCompiledReflectionProbe& probe)
{
    std::uint64_t hash = 1469598103934665603ull;
    HashValue(hash, SectorReflectionProbeBakeVersion);
    HashValue(hash, probe.sourceAuthoringProbeId);
    HashValue(hash, probe.topologySectorId);
    HashValue(hash, probe.enabled);
    HashValue(hash, probe.capturePositionWorld.x);
    HashValue(hash, probe.capturePositionWorld.y);
    HashValue(hash, probe.capturePositionWorld.z);
    HashValue(hash, probe.influenceCenterWorld.x);
    HashValue(hash, probe.influenceCenterWorld.y);
    HashValue(hash, probe.influenceCenterWorld.z);
    HashValue(hash, probe.halfExtentsWorld.x);
    HashValue(hash, probe.halfExtentsWorld.y);
    HashValue(hash, probe.halfExtentsWorld.z);
    HashValue(hash, probe.yawRadians);
    HashValue(hash, probe.priority);
    HashValue(hash, probe.intensity);
    HashValue(hash, probe.resolution);
    HashString(hash, map.bakedLightmap.sourceHash);
    HashValue(hash, map.bakedLightmap.version);
    HashString(hash, map.bakedLightmap.format);
    HashString(hash, map.skySettings.materialId);
    HashValue(hash, map.skySettings.yawOffsetDegrees);
    HashValue(hash, map.skySettings.verticalOffset);
    HashValue(hash, map.skySettings.verticalScale);
    HashValue(hash, map.skySettings.topColor.r);
    HashValue(hash, map.skySettings.topColor.g);
    HashValue(hash, map.skySettings.topColor.b);
    HashValue(hash, map.skySettings.topColor.a);
    std::vector<std::string> materialIds;
    materialIds.reserve(map.resolvedMaterialsById.size());
    for (const auto& entry : map.resolvedMaterialsById) materialIds.push_back(entry.first);
    std::sort(materialIds.begin(), materialIds.end());
    for (const std::string& materialId : materialIds) {
        const SectorMaterialDefinition& material = map.resolvedMaterialsById.at(materialId);
        HashString(hash, materialId);
        HashString(hash, material.path);
        HashAssetFile(hash, material.path);
        const std::string normalPath = SectorMaterialNormalMapPath(material.path);
        HashString(hash, normalPath);
        HashAssetFile(hash, normalPath);
        HashValue(hash, material.metallicFactor);
        HashValue(hash, material.roughnessFactor);
        HashValue(hash, material.normalStrength);
    }
    std::vector<const SectorPlacedRuntimeObject*> captureObjects;
    captureObjects.reserve(map.runtimeObjects.size());
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind == "static_model" || object.kind == "door") {
            captureObjects.push_back(&object);
        }
    }
    std::sort(captureObjects.begin(), captureObjects.end(), [](const auto* a, const auto* b) {
        return a->id < b->id;
    });
    for (const SectorPlacedRuntimeObject* objectPointer : captureObjects) {
        const SectorPlacedRuntimeObject& object = *objectPointer;
        if (object.kind != "static_model" && object.kind != "door") continue;
        HashValue(hash, object.id);
        HashString(hash, object.kind);
        HashString(hash, object.definitionId);
        HashValue(hash, object.position.x);
        HashValue(hash, object.position.y);
        HashValue(hash, object.position.z);
        HashValue(hash, object.yawRadians);
        if (object.kind == "static_model") {
            HashString(hash, object.staticModel.modelPath);
            HashString(hash, object.staticModel.geometryFingerprint);
            HashValue(hash, object.staticModel.scale);
        } else {
            HashString(hash, object.door.materialId);
            HashValue(hash, object.door.anchor.lineDefId);
            HashValue(hash, object.door.anchor.frontSectorId);
            HashValue(hash, object.door.anchor.backSectorId);
            HashValue(hash, object.door.anchor.frontSideDefId);
            HashValue(hash, object.door.anchor.backSideDefId);
            HashValue(hash, object.door.anchor.endpointAX);
            HashValue(hash, object.door.anchor.endpointAY);
            HashValue(hash, object.door.anchor.endpointBX);
            HashValue(hash, object.door.anchor.endpointBY);
            HashValue(hash, object.door.width);
            HashValue(hash, object.door.height);
        }
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

bool ReadSectorReflectionProbeArtifact(
        const std::filesystem::path& path,
        SectorBakedReflectionProbeArtifact& outArtifact,
        std::string& error)
{
    outArtifact = {};
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Could not open reflection probe artifact: " + path.string();
        return false;
    }
    std::array<char, Magic.size()> magic{};
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    std::uint32_t version = 0;
    std::uint32_t count = 0;
    if (!input || magic != Magic || !ReadU32(input, version) || !ReadU32(input, count)
            || version != SectorReflectionProbeBakeVersion || count > MaximumProbeCount) {
        error = "Invalid reflection probe artifact header";
        return false;
    }
    outArtifact.version = static_cast<int>(version);
    outArtifact.probes.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t probeId = 0, resolution = 0, mipCount = 0, hashBytes = 0;
        std::uint32_t halfCount = 0;
        if (!ReadU32(input, probeId) || !ReadU32(input, resolution)
                || !ReadU32(input, mipCount) || !ReadU32(input, hashBytes)
                || !ReadU32(input, halfCount)
                || probeId == 0 || hashBytes == 0 || hashBytes > MaximumHashBytes
                || halfCount != SectorReflectionProbeHalfCount(
                        static_cast<int>(resolution), static_cast<int>(mipCount))) {
            error = "Invalid reflection probe artifact record";
            return false;
        }
        SectorBakedReflectionProbeRecord record;
        record.probeId = static_cast<int>(probeId);
        record.resolution = static_cast<int>(resolution);
        record.mipCount = static_cast<int>(mipCount);
        record.sourceHash.resize(hashBytes);
        input.read(record.sourceHash.data(), static_cast<std::streamsize>(hashBytes));
        record.rgba16.resize(halfCount);
        for (std::uint16_t& half : record.rgba16) {
            if (!ReadU16(input, half)) {
                error = "Truncated reflection probe artifact pixels";
                return false;
            }
        }
        if (!input) {
            error = "Truncated reflection probe artifact";
            return false;
        }
        outArtifact.probes.push_back(std::move(record));
    }
    error.clear();
    return true;
}

bool WriteSectorReflectionProbeArtifact(
        const std::filesystem::path& path,
        const SectorBakedReflectionProbeArtifact& artifact,
        std::string& error)
{
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not create reflection probe artifact: " + temporary.string();
        return false;
    }
    output.write(Magic.data(), static_cast<std::streamsize>(Magic.size()));
    WriteU32(output, SectorReflectionProbeBakeVersion);
    WriteU32(output, static_cast<std::uint32_t>(artifact.probes.size()));
    for (const SectorBakedReflectionProbeRecord& record : artifact.probes) {
        const std::size_t expected = SectorReflectionProbeHalfCount(
                record.resolution, record.mipCount);
        if (record.probeId <= 0 || record.sourceHash.empty()
                || record.sourceHash.size() > MaximumHashBytes
                || record.rgba16.size() != expected
                || expected > std::numeric_limits<std::uint32_t>::max()) {
            error = "Invalid reflection probe record while writing";
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
        WriteU32(output, static_cast<std::uint32_t>(record.probeId));
        WriteU32(output, static_cast<std::uint32_t>(record.resolution));
        WriteU32(output, static_cast<std::uint32_t>(record.mipCount));
        WriteU32(output, static_cast<std::uint32_t>(record.sourceHash.size()));
        WriteU32(output, static_cast<std::uint32_t>(record.rgba16.size()));
        output.write(record.sourceHash.data(),
                static_cast<std::streamsize>(record.sourceHash.size()));
        for (std::uint16_t half : record.rgba16) WriteU16(output, half);
    }
    output.flush();
    if (!output) {
        error = "Failed while writing reflection probe artifact";
        output.close();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    output.close();
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(temporary, path, ec);
    }
    if (ec) {
        error = "Could not install reflection probe artifact: " + ec.message();
        return false;
    }
    error.clear();
    return true;
}

const SectorBakedReflectionProbeRecord* FindSectorBakedReflectionProbeRecord(
        const SectorBakedReflectionProbeArtifact& artifact,
        int probeId)
{
    const auto it = std::find_if(
            artifact.probes.begin(), artifact.probes.end(),
            [probeId](const SectorBakedReflectionProbeRecord& record) {
                return record.probeId == probeId;
            });
    return it == artifact.probes.end() ? nullptr : &*it;
}

bool BuildSectorReflectionProbeRecord(
        int probeId,
        int resolution,
        const std::string& sourceHash,
        const std::vector<Vector4>& capturedFaces,
        SectorBakedReflectionProbeRecord& outRecord,
        std::string& error)
{
    outRecord = {};
    const int mipCount = SectorReflectionProbeMipCount(resolution);
    const std::size_t facePixels = static_cast<std::size_t>(resolution)
            * static_cast<std::size_t>(resolution);
    if (probeId <= 0 || sourceHash.empty() || mipCount == 0
            || capturedFaces.size() != facePixels * 6u) {
        error = "Invalid reflection probe capture input";
        return false;
    }
    outRecord.probeId = probeId;
    outRecord.resolution = resolution;
    outRecord.mipCount = mipCount;
    outRecord.sourceHash = sourceHash;
    outRecord.rgba16.reserve(SectorReflectionProbeHalfCount(resolution, mipCount));
    const auto appendHalf = [&](Vector4 color) {
        outRecord.rgba16.push_back(SectorLightmapFloatToBinary16(std::max(0.0f, color.x)));
        outRecord.rgba16.push_back(SectorLightmapFloatToBinary16(std::max(0.0f, color.y)));
        outRecord.rgba16.push_back(SectorLightmapFloatToBinary16(std::max(0.0f, color.z)));
        outRecord.rgba16.push_back(SectorLightmapFloatToBinary16(1.0f));
    };
    for (const Vector4 color : capturedFaces) appendHalf(color);
    constexpr std::uint32_t SampleCount = 64;
    for (int mip = 1, size = resolution / 2; mip < mipCount;
            ++mip, size = std::max(1, size / 2)) {
        const float roughness = static_cast<float>(mip)
                / static_cast<float>(mipCount - 1);
        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const Vector3 normal = CubemapDirection(face, x, y, size);
                    Vector4 sum{};
                    float weight = 0.0f;
                    for (std::uint32_t sample = 0; sample < SampleCount; ++sample) {
                        const Vector3 halfVector = ImportanceSampleGgx(
                                static_cast<float>(sample) / SampleCount,
                                RadicalInverseVdc(sample), roughness, normal);
                        const Vector3 lightDirection = Vector3Normalize(Vector3Subtract(
                                Vector3Scale(halfVector,
                                        2.0f * Vector3DotProduct(normal, halfVector)),
                                normal));
                        const float ndotl = std::max(
                                Vector3DotProduct(normal, lightDirection), 0.0f);
                        if (ndotl <= 0.0f) continue;
                        sum = Vector4Add(sum, Vector4Scale(
                                SampleCapturedCubemap(capturedFaces, resolution,
                                        lightDirection), ndotl));
                        weight += ndotl;
                    }
                    appendHalf(weight > 0.0f
                            ? Vector4Scale(sum, 1.0f / weight)
                            : SampleCapturedCubemap(capturedFaces, resolution, normal));
                }
            }
        }
    }
    error.clear();
    return true;
}

} // namespace game
