#pragma once

#include "sector_demo/SectorReflectionProbeTypes.h"

#include <filesystem>
#include <string>
#include <vector>

namespace game {

struct SectorTopologyMap;

int SectorReflectionProbeMipCount(int resolution);
std::size_t SectorReflectionProbeHalfCount(int resolution, int mipCount);

std::string ComputeSectorReflectionProbeSourceHash(
        const SectorTopologyMap& map,
        const SectorCompiledReflectionProbe& probe);

bool ReadSectorReflectionProbeArtifact(
        const std::filesystem::path& path,
        SectorBakedReflectionProbeArtifact& outArtifact,
        std::string& error);

bool WriteSectorReflectionProbeArtifact(
        const std::filesystem::path& path,
        const SectorBakedReflectionProbeArtifact& artifact,
        std::string& error);

const SectorBakedReflectionProbeRecord* FindSectorBakedReflectionProbeRecord(
        const SectorBakedReflectionProbeArtifact& artifact,
        int probeId);

bool BuildSectorReflectionProbeRecord(
        int probeId,
        int resolution,
        const std::string& sourceHash,
        const std::vector<Vector4>& capturedFaces,
        SectorBakedReflectionProbeRecord& outRecord,
        std::string& error);

} // namespace game
