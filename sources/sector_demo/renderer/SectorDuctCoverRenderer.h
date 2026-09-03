#pragma once

#include "sector_demo/renderer/SectorDoorRenderer.h"

#include <raylib.h>

#include <unordered_map>

namespace game {

class SectorDuctCoverRenderer {
public:
    bool Initialize();
    void Shutdown();
    void Draw(
            const SectorDoorDrawContext& context,
            SectorDoorRenderer& opaqueRenderer);

private:
    struct MeshCacheEntry {
        Mesh frame = {};
        Mesh louvers = {};
        float width = 0.0f;
        float height = 0.0f;
        float thickness = 0.0f;
        float border = 0.0f;
        float angleDegrees = 0.0f;
        int louverCount = 0;
        bool seenThisFrame = false;
    };

    std::unordered_map<int, MeshCacheEntry> meshCache;
    bool loaded = false;
};

} // namespace game
