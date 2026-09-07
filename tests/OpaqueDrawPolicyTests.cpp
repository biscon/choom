#include "sector_demo/renderer/SectorOpaqueDrawPolicy.h"
#include "sector_demo/renderer/SectorShaderSource.h"
#include "engine/assets/ModelMaterialMetadata.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>

namespace
{
void TestShaderPreamble()
{
    for (const std::string leading : {"", "\n", "\n\n", "\r\n"}) {
        for (int variant = 0; variant < 3; ++variant) {
            const std::string define = "#define WINDOW_FLAT_PASS " + std::to_string(variant) + "\n";
            const std::string source = leading + "#version 330\nvoid main() {}\n";
            const auto shared = game::InsertSectorShaderPreamble(source, "// reflection functions\n");
            const auto result = game::InsertSectorShaderPreamble(shared, define);
            assert(result == leading + "#version 330\n" + define +
                "// reflection functions\nvoid main() {}\n");
        }
    }
    assert(game::InsertSectorShaderPreamble("#version 330", "// test\n") ==
        "#version 330\n// test\n");
}

void TestMaterialMetadata()
{
    const char *json = R"({"asset":{"version":"2.0"},"materials":[{},
        {"doubleSided":true},{"doubleSided":false,"alphaMode":"MASK"},{"alphaMode":"BLEND"}]})";
    cgltf_options options{};
    cgltf_data *data = nullptr;
    assert(cgltf_parse(&options, json, std::strlen(json), &data) == cgltf_result_success);
    engine::ModelMaterialAsset material;
    assert(game::SectorMaterialCullWinding(material, MatrixIdentity()) == 0);
    assert(!game::SectorMaterialDepthEligible(material));
    engine::ReadGltfRasterMetadata(material, nullptr);
    assert(game::SectorMaterialDepthEligible(material));
    for (int i = 0; i < 4; ++i) {
        engine::ReadGltfRasterMetadata(material, &data->materials[i]);
        assert(game::SectorMaterialDepthEligible(material) == (i < 2));
        const int winding = i == 1 ? 0 : 1;
        assert(game::SectorMaterialCullWinding(material, MatrixIdentity()) == winding);
        assert(game::SectorMaterialCullWinding(material, MatrixScale(-1, 2, 3)) == -winding);
        assert(game::SectorMaterialCullWinding(material, MatrixScale(-1, -2, 3)) == winding);
        assert(game::SectorMaterialCullWinding(material, MatrixScale(0, 1, 1)) == 0);
        assert(game::SectorMaterialCullWinding(material, MatrixScale(NAN, 1, 1)) == 0);
    }
    cgltf_free(data);
}

void TestVisibilityAndOrdering()
{
    const Camera3D camera{{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, 90, CAMERA_PERSPECTIVE};
    const BoundingBox visible{{-1, -1, 3}, {1, 1, 4}};
    assert(game::SectorBoundsInView(camera, 1, 0.1f, 100, visible));
    assert(!game::SectorBoundsInView(camera, 1, 0.1f, 100, {{-1, -1, -4}, {1, 1, -3}}));
    assert(!game::SectorBoundsInView(camera, 1, 0.1f, 100, {{20, -1, 3}, {21, 1, 4}}));
    assert(game::SectorBoundsInView(camera, 1, 0.1f, 100, {{-1, -1, -1}, {1, 1, 1}}));
    assert(game::SectorBoundsInView(camera, 1, 0.1f, 100, {{4, -1, 3}, {5, 1, 4}}));
    assert(game::SectorBoundsInView(camera, 1, 0.1f, 100, {{NAN, 0, 0}, {1, 1, 1}}));
    assert(game::SectorBoundsInView(camera, 0, 0.1f, 100, visible));
    std::vector<game::SectorOpaqueDrawItem> items(3);
    items[0].id = 3;
    items[0].depth = 2;
    items[1].id = 2;
    items[1].depth = 1;
    items[2].id = 1;
    items[2].depth = 1;
    std::sort(items.begin(), items.end(), game::SectorOpaqueDrawLess);
    assert(items[0].id == 1 && items[1].id == 2 && items[2].id == 3);
    assert(game::SectorNearestViewDepth(camera, visible) == 3);
    game::RuntimePortalVisibilityResult visibility;
    visibility.validStartSector = true;
    visibility.visibleSectorIds = {10};
    assert(game::SectorPaneVisible(true, true, 10, 20, &visibility, camera, 1, 0.1f, 100, visible));
    assert(game::SectorPaneVisible(true, true, 20, 10, &visibility, camera, 1, 0.1f, 100, visible));
    assert(
        !game::SectorPaneVisible(true, true, 20, 30, &visibility, camera, 1, 0.1f, 100, visible));
    assert(
        !game::SectorPaneVisible(false, true, 10, 20, &visibility, camera, 1, 0.1f, 100, visible));
    assert(
        !game::SectorPaneVisible(true, false, 10, 20, &visibility, camera, 1, 0.1f, 100, visible));
    assert(!game::SectorPaneVisible(true, true, 10, 20, &visibility, camera, 1, 0.1f, 100,
                                    {{-1, -1, -4}, {1, 1, -3}}));
    // Shadow participation is deliberately not an input to main-view/depth eligibility.
    assert(
        game::SectorOpaqueModelVisible(true, 10, visibility, camera, 1, .1f, 100, visible, true));
    assert(
        !game::SectorOpaqueModelVisible(true, 20, visibility, camera, 1, .1f, 100, visible, true));
    assert(
        !game::SectorOpaqueModelVisible(false, 10, visibility, camera, 1, .1f, 100, visible, true));
    assert(game::SectorOpaqueModelVisible(true, 10, visibility, camera, 1, .1f, 100, {}, false));
}

float Fract(float x)
{
    return x - std::floor(x);
}
float Hash(float x, float y)
{
    Vector3 p{Fract(x * 0.1031f), Fract(y * 0.1031f), Fract(x * 0.1031f)};
    float d = p.x * (p.y + 33.33f) + p.y * (p.z + 33.33f) + p.z * (p.x + 33.33f);
    p = Vector3AddValue(p, d);
    return Fract((p.x + p.y) * p.z);
}
Vector4 Hashes(Vector2 cell)
{
    return {Hash(cell.x, cell.y), Hash(cell.x + 1, cell.y), Hash(cell.x, cell.y + 1),
            Hash(cell.x + 1, cell.y + 1)};
}
float FromCell(Vector2 position, Vector4 h)
{
    float x = Fract(position.x), y = Fract(position.y);
    x = x * x * (3 - 2 * x);
    y = y * y * (3 - 2 * y);
    return Lerp(Lerp(h.x, h.y, x), Lerp(h.z, h.w, x), y);
}
Vector2 Cell(Vector2 p)
{
    return {std::floor(p.x), std::floor(p.y)};
}
float Noise(Vector2 p)
{
    return FromCell(p, Hashes(Cell(p)));
}
Vector3 SharedNoise(Vector2 p, Vector2 x, Vector2 y)
{
    const Vector2 cell = Cell(p);
    const Vector4 hashes = Hashes(cell);
    const auto sample = [&](Vector2 v) {
        const Vector2 other = Cell(v);
        return other.x == cell.x && other.y == cell.y ? FromCell(v, hashes) : Noise(v);
    };
    return {FromCell(p, hashes), sample(x), sample(y)};
}
void TestGlassNoiseEquivalence()
{
    // Same finite-difference positions and frequencies as the shader. Include
    // negative cells, crossings, and large stable object-ID seed offsets.
    for (int seed : {0, 1, 200, 9999})
        for (int i = -1000; i <= 1000; ++i) {
            const Vector2 position{i * 0.017f, i * -0.023f};
            for (float frequency : {0.65f, 5.0f}) {
                const float seedScale = frequency == 5 ? 1.731f : 1;
                const Vector2 offset{seed * 0.754877666f * seedScale,
                                     seed * 0.569840296f * seedScale};
                const Vector2 p = Vector2Add(Vector2Scale(position, frequency), offset);
                const Vector2 x =
                    Vector2Add(Vector2Scale(Vector2Add(position, {0.025f, 0}), frequency), offset);
                const Vector2 y =
                    Vector2Add(Vector2Scale(Vector2Add(position, {0, 0.025f}), frequency), offset);
                const Vector3 shared = SharedNoise(p, x, y);
                assert(shared.x == Noise(p) && shared.y == Noise(x) && shared.z == Noise(y));
            }
        }
    std::ifstream file(GLASS_SHADER_SOURCE_PATH);
    const std::string shader((std::istreambuf_iterator<char>(file)), {});
    assert(shader.find("vec3 samples = GlassPatternSamples(panePosition, derivativeStep)") !=
           std::string::npos);
    assert(shader.find("if (glassImperfectionStrength <= 0.0)") != std::string::npos);
    assert(shader.find("if (environmentSpecularScale > 0.0)") != std::string::npos);
    assert(shader.find("#if WINDOW_FLAT_PASS == 0\n    if (advancedTransmission != 0)") !=
           std::string::npos);
}
} // namespace
int main()
{
    TestShaderPreamble();
    TestMaterialMetadata();
    TestVisibilityAndOrdering();
    TestGlassNoiseEquivalence();
}
