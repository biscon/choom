#include "sector_demo/SectorSwingDoorCatalog.h"

#include "util/json.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

using Json = nlohmann::ordered_json;

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool Near(float actual, float expected, float epsilon = 0.00001f)
{
    return std::fabs(actual - expected) <= epsilon;
}

Json MakeCatalog()
{
    return Json{
            {"formatVersion", 1},
            {"assets", Json::array({
                    Json{
                            {"id", "test_leaf"},
                            {"displayName", "Test Leaf"},
                            {"leafModelPath", "assets/models/doors/swing/test_leaf.gltf"},
                            {"nominalWidth", 1.0f},
                            {"nominalHeight", 2.0f},
                            {"nominalThickness", 0.1f},
                            {"sourcePack", "test_pack.glb"},
                            {"frameModelPath", "assets/models/doors/swing/test_frame.gltf"},
                            {"frameOuterWidth", 1.2f},
                            {"frameOuterHeight", 2.2f}
                    }
            })}
    };
}

void ExpectRejected(Json catalog, const char* description)
{
    game::SectorSwingDoorCatalog parsed;
    std::string error;
    Check(!game::ParseSectorSwingDoorCatalogJson(catalog.dump(), parsed, error), description);
    Check(!error.empty(), "rejected catalog reports an error");
    Check(parsed.assets.empty() && parsed.assetIndexById.empty(),
          "rejected catalog clears partial output");
}

void TestGeneratedCatalogLoadsAndLooksUpByStableId()
{
    game::SectorSwingDoorCatalog catalog;
    std::string error;
    Check(game::LoadSectorSwingDoorCatalog(
                  std::string{ASSETS_PATH} + "models/doors/swing/catalog.json",
                  catalog,
                  error),
          "generated swing door catalog loads");
    Check(error.empty() && catalog.formatVersion == 1 && catalog.assets.size() == 20,
          "generated swing door catalog has expected version and style count");

    game::SectorSwingDoorCatalogAsset asset;
    Check(game::FindSectorSwingDoorCatalogAsset(
                  catalog, "wooden_interior_001", asset),
          "stable catalog ID lookup resolves generated style");
    Check(asset.id == "wooden_interior_001"
                  && asset.hasFrame
                  && asset.nominalWidth > 0.0f
                  && asset.nominalHeight > 0.0f
                  && asset.nominalThickness > 0.0f,
          "generated style retains leaf and frame CPU metadata");
    Check(!game::FindSectorSwingDoorCatalogAsset(
                  catalog, "unknown_style", asset),
          "unknown catalog ID lookup fails without invalidating catalog");
}

void TestCatalogValidation()
{
    game::SectorSwingDoorCatalog parsed;
    std::string error;
    const Json valid = MakeCatalog();
    Check(game::ParseSectorSwingDoorCatalogJson(valid.dump(), parsed, error),
          "minimal valid catalog parses");
    Check(parsed.assets.size() == 1 && parsed.assets[0].hasFrame,
          "valid catalog records complete frame metadata");

    Json invalid = valid;
    invalid["formatVersion"] = 2;
    ExpectRejected(invalid, "unsupported catalog version is rejected");

    invalid = valid;
    invalid["assets"].push_back(invalid["assets"][0]);
    ExpectRejected(invalid, "duplicate catalog IDs are rejected");

    invalid = valid;
    invalid["assets"][0]["id"] = "";
    ExpectRejected(invalid, "empty catalog ID is rejected");

    invalid = valid;
    invalid["assets"][0]["nominalWidth"] = 0.0f;
    ExpectRejected(invalid, "non-positive nominal width is rejected");

    invalid = valid;
    invalid["assets"][0]["nominalHeight"] = "large";
    ExpectRejected(invalid, "non-numeric nominal height is rejected");

    invalid = valid;
    invalid["assets"][0]["leafModelPath"] =
            "assets/models/doors/swing/../outside.gltf";
    ExpectRejected(invalid, "escaping leaf path is rejected");

    invalid = valid;
    invalid["assets"][0]["leafModelPath"] =
            "assets/models/doors/swing/test_leaf.glb";
    ExpectRejected(invalid, "non-gltf leaf path is rejected");

    invalid = valid;
    invalid["assets"][0].erase("frameOuterHeight");
    ExpectRejected(invalid, "partial frame metadata is rejected");

    invalid = valid;
    invalid["assets"][0]["unexpected"] = true;
    ExpectRejected(invalid, "unknown catalog asset fields are rejected");
}

game::SectorSwingDoorCatalogAsset MakeFitAsset()
{
    game::SectorSwingDoorCatalogAsset asset;
    asset.nominalWidth = 1.0f;
    asset.nominalHeight = 2.0f;
    asset.nominalThickness = 0.1f;
    return asset;
}

void TestUniformFitCalculations()
{
    const game::SectorSwingDoorCatalogAsset asset = MakeFitAsset();

    const game::SectorSwingDoorFitResult manual =
            game::ComputeSectorSwingDoorFit(
                    asset, 2.0f, 3.0f, game::SectorDoorModelFit::Manual, 1.5f);
    Check(manual.status == game::SectorSwingDoorFitStatus::Valid
                  && Near(manual.effectiveScale, 1.5f)
                  && Near(manual.actualWidth, 1.5f)
                  && Near(manual.actualHeight, 3.0f)
                  && Near(manual.actualThickness, 0.15f)
                  && Near(manual.widthGap, 0.5f)
                  && Near(manual.heightGap, 0.0f),
          "manual fit uses one uniform authored scale");

    const game::SectorSwingDoorFitResult width =
            game::ComputeSectorSwingDoorFit(
                    asset, 2.0f, 3.0f, game::SectorDoorModelFit::FitWidth, 1.0f);
    Check(width.status == game::SectorSwingDoorFitStatus::HeightOverflow
                  && Near(width.effectiveScale, 2.0f)
                  && Near(width.actualWidth, 2.0f)
                  && Near(width.actualHeight, 4.0f)
                  && Near(width.heightGap, -1.0f),
          "fit-width preserves uniform scale and reports short-portal overflow");

    const game::SectorSwingDoorFitResult inside =
            game::ComputeSectorSwingDoorFit(
                    asset, 3.0f, 3.0f, game::SectorDoorModelFit::FitInside, 1.0f);
    Check(inside.status == game::SectorSwingDoorFitStatus::Valid
                  && Near(inside.effectiveScale, 1.5f)
                  && Near(inside.actualWidth, 1.5f)
                  && Near(inside.actualHeight, 3.0f)
                  && Near(inside.widthGap, 1.5f),
          "fit-inside chooses the smaller width/height ratio uniformly");

    const game::SectorSwingDoorFitResult multiplied =
            game::ComputeSectorSwingDoorFit(
                    asset, 1.0f, 2.0f, game::SectorDoorModelFit::FitInside, 1.25f);
    Check(multiplied.status == game::SectorSwingDoorFitStatus::WidthAndHeightOverflow
                  && Near(multiplied.effectiveScale, 1.25f)
                  && Near(multiplied.actualThickness, 0.125f),
          "fit multiplier remains uniform and can deliberately overflow both axes");
}

void TestFitRejectsInvalidInputs()
{
    game::SectorSwingDoorCatalogAsset asset = MakeFitAsset();
    Check(game::ComputeSectorSwingDoorFit(
                  asset, 0.0f, 2.0f, game::SectorDoorModelFit::Manual, 1.0f).status
                  == game::SectorSwingDoorFitStatus::InvalidInput,
          "fit rejects non-positive target dimensions");
    Check(game::ComputeSectorSwingDoorFit(
                  asset, 1.0f, 2.0f, game::SectorDoorModelFit::Manual, 0.0f).status
                  == game::SectorSwingDoorFitStatus::InvalidInput,
          "fit rejects non-positive scale");
    asset.nominalThickness = std::numeric_limits<float>::infinity();
    Check(game::ComputeSectorSwingDoorFit(
                  asset, 1.0f, 2.0f, game::SectorDoorModelFit::FitInside, 1.0f).status
                  == game::SectorSwingDoorFitStatus::InvalidInput,
          "fit rejects non-finite nominal dimensions");
}

} // namespace

int main()
{
    TestGeneratedCatalogLoadsAndLooksUpByStableId();
    TestCatalogValidation();
    TestUniformFitCalculations();
    TestFitRejectsInvalidInputs();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All sector swing door catalog tests passed\n";
    return 0;
}
