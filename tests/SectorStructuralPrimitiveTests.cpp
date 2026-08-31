#include "sector_demo/SectorStructuralPrimitives.h"
#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void Require(bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

} // namespace

int main()
{
    using namespace game;

    const SectorStructuralPrimitiveKind kinds[] = {
            SectorStructuralPrimitiveKind::Box,
            SectorStructuralPrimitiveKind::Ramp,
            SectorStructuralPrimitiveKind::Stairs,
            SectorStructuralPrimitiveKind::Cylinder,
            SectorStructuralPrimitiveKind::Sphere};
    std::vector<SectorAuthoringStructuralPrimitive> authored;
    for (int index = 0; index < 5; ++index) {
        SectorAuthoringStructuralPrimitive primitive =
                DefaultSectorAuthoringStructuralPrimitive(kinds[index]);
        primitive.id = 50 - index;
        authored.push_back(primitive);
    }
    Require(!authored.back().collision,
            "sphere collision must default off");
    Require(ValidateSectorAuthoringStructuralPrimitives(authored).empty(),
            "default primitives must validate");

    SectorTopologyMap emptyMap;
    std::vector<SectorCompiledStructuralPrimitive> first;
    std::vector<SectorStructuralDiagnostic> diagnostics;
    Require(CompileSectorStructuralPrimitives(
                    authored, emptyMap, first, diagnostics),
            "all primitive kinds must compile");
    Require(first.size() == authored.size(),
            "every enabled primitive must produce compiled data");
    for (size_t index = 0; index < first.size(); ++index) {
        Require(!first[index].surfaces.empty(),
                "every primitive kind must produce surfaces");
        Require(!first[index].geometryFingerprint.empty(),
                "compiled geometry must have a fingerprint");
        if (index > 0) {
            Require(first[index - 1].sourceAuthoringPrimitiveId
                            < first[index].sourceAuthoringPrimitiveId,
                    "compiled primitives must be ordered by stable ID");
        }
        for (const SectorCompiledStructuralSurface& surface : first[index].surfaces) {
            Require(surface.face.primitiveId
                            == first[index].sourceAuthoringPrimitiveId,
                    "semantic face identity must retain the primitive ID");
            Require(surface.vertices.size() % 3 == 0,
                    "compiled surfaces must contain complete triangles");
        }
    }

    std::reverse(authored.begin(), authored.end());
    std::vector<SectorCompiledStructuralPrimitive> second;
    diagnostics.clear();
    Require(CompileSectorStructuralPrimitives(
                    authored, emptyMap, second, diagnostics),
            "reordered authoring input must compile");
    Require(second.size() == first.size(),
            "deterministic compile must retain primitive count");
    for (size_t index = 0; index < first.size(); ++index) {
        Require(first[index].sourceAuthoringPrimitiveId
                        == second[index].sourceAuthoringPrimitiveId
                        && first[index].geometryFingerprint
                                == second[index].geometryFingerprint,
                "compile order and fingerprints must be deterministic");
    }

    authored.front().id = authored.back().id;
    const auto invalid = ValidateSectorAuthoringStructuralPrimitives(authored);
    Require(!invalid.empty(), "duplicate stable IDs must fail validation");

    std::cout << "Sector structural primitive tests passed\n";
    return 0;
}
