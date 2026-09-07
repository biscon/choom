#include <external/glad.h>
#include "sector_demo/renderer/SectorOpaqueDrawPolicy.h"
#include <rlgl.h>

namespace game
{
void ApplySectorMaterialCulling(const engine::ModelMaterialAsset &material, Matrix transform)
{
    const int winding = SectorMaterialCullWinding(material, transform);
    glFrontFace(winding < 0 ? GL_CW : GL_CCW);
    if (winding == 0)
        rlDisableBackfaceCulling();
    else
        rlEnableBackfaceCulling();
}
void RestoreSectorMaterialCulling()
{
    glFrontFace(GL_CCW);
    rlEnableBackfaceCulling();
}
} // namespace game
