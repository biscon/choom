#pragma once
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
namespace game
{
struct SectorEditorPathEditingState
{
    std::vector<SectorPathWaypoint> pending;
    int waypointId = 0;
    bool moving = false;
    bool moveArmed = false;
    int armedPathId = 0;
    int armedWaypointId = 0;
    Vector2 pressScreen = {};
    SectorAuthoringPath original;
    SectorAuthoringPath preview;
    SectorTopologyCoordPoint press = {};
    int bufferedPathId = 0;
    char idBuffer[64] = {};
};
struct SectorEditorPathEditingContext
{
    SectorEditorState &state;
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap &map;
    SectorAuthoringGraph &graph;
    SectorEditorDerivationDocumentAccess derivation;
    SelectionState &selection;
    SectorEditorPathEditingState &editing;
    std::string &status;
};
class SectorEditorPathEditingService
{
  public:
    explicit SectorEditorPathEditingService(SectorEditorPathEditingContext context)
        : context_(context)
    {
    }
    SectorAuthoringPath *Selected();
    const SectorAuthoringPath *Selected() const;
    SectorEditorPathEditingState &State() { return context_.editing; }
    bool Select(int id);
    bool Create();
    bool Rename(const std::string &name);
    bool Delete();
    bool Dissolve();
    bool Insert(size_t segment, SectorTopologyCoordPoint point);
    bool ArmMove(int waypointId, Vector2 screen, SectorTopologyCoordPoint point);
    bool UpdateMoveArm(Vector2 screen);
    bool BeginMove(SectorTopologyCoordPoint point);
    void Move(SectorTopologyCoordPoint point);
    bool FinishMove();
    void Cancel();

  private:
    bool Commit(SectorAuthoringGraph candidate, const char *status);
    bool Replace(const SectorAuthoringPath &path, const char *status);
    SectorEditorPathEditingContext context_;
};
} // namespace game
