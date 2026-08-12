#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace game {

constexpr const char* kSectorSwingDoorCatalogAssetPath =
        "assets/models/doors/swing/catalog.json";

struct SectorSwingDoorCatalogAsset {
    std::string id;
    std::string displayName;
    std::string leafModelPath;
    std::string frameModelPath;
    std::string sourcePack;
    float nominalWidth = 0.0f;
    float nominalHeight = 0.0f;
    float nominalThickness = 0.0f;
    float frameOuterWidth = 0.0f;
    float frameOuterHeight = 0.0f;
    float leafHingeToFrameCenter = 0.0f;
    float leafBottomOffset = 0.0f;
    bool hasFrame = false;
    bool hasFrameAlignment = false;
};

struct SectorSwingDoorCatalog {
    int formatVersion = 0;
    std::vector<SectorSwingDoorCatalogAsset> assets;
    std::unordered_map<std::string, size_t> assetIndexById;
};

enum class SectorSwingDoorFitStatus {
    Valid,
    WidthOverflow,
    HeightOverflow,
    WidthAndHeightOverflow,
    InvalidInput
};

struct SectorSwingDoorFitResult {
    SectorSwingDoorFitStatus status = SectorSwingDoorFitStatus::InvalidInput;
    float effectiveScale = 0.0f;
    float actualWidth = 0.0f;
    float actualHeight = 0.0f;
    float actualThickness = 0.0f;
    float assemblyWidth = 0.0f;
    float assemblyHeight = 0.0f;
    float widthGap = 0.0f;
    float heightGap = 0.0f;
};

bool ParseSectorSwingDoorCatalogJson(
        std::string_view jsonText,
        SectorSwingDoorCatalog& outCatalog,
        std::string& outError);

bool LoadSectorSwingDoorCatalog(
        const std::string& path,
        SectorSwingDoorCatalog& outCatalog,
        std::string& outError);

bool FindSectorSwingDoorCatalogAsset(
        const SectorSwingDoorCatalog& catalog,
        std::string_view id,
        SectorSwingDoorCatalogAsset& outAsset);

SectorSwingDoorFitResult ComputeSectorSwingDoorFit(
        const SectorSwingDoorCatalogAsset& asset,
        float targetWidth,
        float targetHeight,
        SectorDoorModelFit fit,
        float modelScale);

const char* SectorSwingDoorFitStatusName(SectorSwingDoorFitStatus status);

} // namespace game
