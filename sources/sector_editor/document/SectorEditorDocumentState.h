#pragma once

#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorTopologyMap.h"

#include <optional>
#include <string>

namespace game {

enum class SectorEditorAuthoringDerivationState {
    InvalidNoDerived,
    ValidCurrent,
    ValidStale,
    InvalidLastValid
};

struct SectorEditorAuthoringDocumentAccess {
    SectorAuthoringGraph& graph;
};

struct SectorEditorConstAuthoringDocumentAccess {
    const SectorAuthoringGraph& graph;
};

struct SectorEditorConstDerivationDocumentAccess;
struct SectorEditorConstDocumentLifecycleAccess;

struct SectorEditorDocumentMapAccess {
    SectorTopologyMap& topologyMap;
};

struct SectorEditorConstDocumentMapAccess {
    const SectorTopologyMap& topologyMap;
};

struct SectorEditorDerivationDocumentAccess {
    SectorAuthoringDerivationResult& authoringDerivation;
    std::optional<SectorTopologyMap>& lastValidAuthoringDerivedTopology;
    SectorEditorAuthoringDerivationState& authoringDerivationState;
    bool& authoringDerivedTopologyStale;
    std::string& authoringDerivationStatus;

    operator SectorEditorConstDerivationDocumentAccess() const;
};

struct SectorEditorConstDerivationDocumentAccess {
    const SectorAuthoringDerivationResult& authoringDerivation;
    const std::optional<SectorTopologyMap>& lastValidAuthoringDerivedTopology;
    SectorEditorAuthoringDerivationState authoringDerivationState;
    bool authoringDerivedTopologyStale;
    const std::string& authoringDerivationStatus;
};

struct SectorEditorDocumentLifecycleAccess {
    bool& topologyDocumentInitialized;
    bool& topologyDocumentDirty;
    std::string& topologyDocumentStatus;
    std::string& currentLevelName;
    std::string& currentLevelPath;
    bool& hasCurrentLevelPath;
    bool& hasUnsavedChanges;

    operator SectorEditorConstDocumentLifecycleAccess() const;
};

struct SectorEditorConstDocumentLifecycleAccess {
    bool topologyDocumentInitialized;
    bool topologyDocumentDirty;
    const std::string& topologyDocumentStatus;
    const std::string& currentLevelName;
    const std::string& currentLevelPath;
    bool hasCurrentLevelPath;
    bool hasUnsavedChanges;
};

struct SectorEditorAuthoringDocumentState {
    SectorAuthoringGraph authoringGraph;
};

struct SectorEditorDerivationState {
    SectorAuthoringDerivationResult authoringDerivation;
    std::optional<SectorTopologyMap> lastValidAuthoringDerivedTopology;
    SectorEditorAuthoringDerivationState authoringDerivationState =
            SectorEditorAuthoringDerivationState::InvalidNoDerived;
    bool authoringDerivedTopologyStale = true;
    std::string authoringDerivationStatus;
};

struct SectorEditorDocumentMapState {
    SectorTopologyMap topologyMap;
};

struct SectorEditorDocumentLifecycleState {
    bool topologyDocumentInitialized = false;
    bool topologyDocumentDirty = false;
    std::string topologyDocumentStatus;
    std::string currentLevelName;
    std::string currentLevelPath;
    bool hasCurrentLevelPath = false;
    bool hasUnsavedChanges = false;
};

struct SectorEditorDocumentState {
    SectorEditorAuthoringDocumentState authoring;
    SectorEditorDerivationState derivation;
    SectorEditorDocumentMapState map;
    SectorEditorDocumentLifecycleState lifecycle;
};

inline SectorEditorAuthoringDocumentAccess MakeSectorEditorAuthoringDocumentAccess(
        SectorAuthoringGraph& graph)
{
    return SectorEditorAuthoringDocumentAccess{graph};
}

inline SectorEditorConstAuthoringDocumentAccess MakeSectorEditorAuthoringDocumentAccess(
        const SectorAuthoringGraph& graph)
{
    return SectorEditorConstAuthoringDocumentAccess{graph};
}

inline SectorEditorDocumentMapAccess MakeSectorEditorDocumentMapAccess(
        SectorTopologyMap& topologyMap)
{
    return SectorEditorDocumentMapAccess{topologyMap};
}

inline SectorEditorConstDocumentMapAccess MakeSectorEditorDocumentMapAccess(
        const SectorTopologyMap& topologyMap)
{
    return SectorEditorConstDocumentMapAccess{topologyMap};
}

inline SectorEditorDocumentMapAccess MakeSectorEditorDocumentMapAccess(
        SectorEditorDocumentMapState& map)
{
    return MakeSectorEditorDocumentMapAccess(map.topologyMap);
}

inline SectorEditorConstDocumentMapAccess MakeSectorEditorDocumentMapAccess(
        const SectorEditorDocumentMapState& map)
{
    return MakeSectorEditorDocumentMapAccess(map.topologyMap);
}

inline SectorEditorDerivationDocumentAccess MakeSectorEditorDerivationDocumentAccess(
        SectorAuthoringDerivationResult& authoringDerivation,
        std::optional<SectorTopologyMap>& lastValidAuthoringDerivedTopology,
        SectorEditorAuthoringDerivationState& authoringDerivationState,
        bool& authoringDerivedTopologyStale,
        std::string& authoringDerivationStatus)
{
    return SectorEditorDerivationDocumentAccess{
            authoringDerivation,
            lastValidAuthoringDerivedTopology,
            authoringDerivationState,
            authoringDerivedTopologyStale,
            authoringDerivationStatus};
}

inline SectorEditorDerivationDocumentAccess MakeSectorEditorDerivationDocumentAccess(
        SectorEditorDerivationState& derivation)
{
    return MakeSectorEditorDerivationDocumentAccess(
            derivation.authoringDerivation,
            derivation.lastValidAuthoringDerivedTopology,
            derivation.authoringDerivationState,
            derivation.authoringDerivedTopologyStale,
            derivation.authoringDerivationStatus);
}

inline SectorEditorConstDerivationDocumentAccess MakeSectorEditorDerivationDocumentAccess(
        const SectorAuthoringDerivationResult& authoringDerivation,
        const std::optional<SectorTopologyMap>& lastValidAuthoringDerivedTopology,
        SectorEditorAuthoringDerivationState authoringDerivationState,
        bool authoringDerivedTopologyStale,
        const std::string& authoringDerivationStatus)
{
    return SectorEditorConstDerivationDocumentAccess{
            authoringDerivation,
            lastValidAuthoringDerivedTopology,
            authoringDerivationState,
            authoringDerivedTopologyStale,
            authoringDerivationStatus};
}

inline SectorEditorConstDerivationDocumentAccess MakeSectorEditorDerivationDocumentAccess(
        const SectorEditorDerivationState& derivation)
{
    return MakeSectorEditorDerivationDocumentAccess(
            derivation.authoringDerivation,
            derivation.lastValidAuthoringDerivedTopology,
            derivation.authoringDerivationState,
            derivation.authoringDerivedTopologyStale,
            derivation.authoringDerivationStatus);
}

inline SectorEditorConstDerivationDocumentAccess MakeSectorEditorConstDerivationDocumentAccess(
        SectorEditorDerivationDocumentAccess derivation)
{
    return SectorEditorConstDerivationDocumentAccess{
            derivation.authoringDerivation,
            derivation.lastValidAuthoringDerivedTopology,
            derivation.authoringDerivationState,
            derivation.authoringDerivedTopologyStale,
            derivation.authoringDerivationStatus};
}

inline SectorEditorDerivationDocumentAccess::operator SectorEditorConstDerivationDocumentAccess() const
{
    return MakeSectorEditorConstDerivationDocumentAccess(*this);
}

inline SectorEditorDocumentLifecycleAccess MakeSectorEditorDocumentLifecycleAccess(
        SectorEditorDocumentLifecycleState& lifecycle)
{
    return SectorEditorDocumentLifecycleAccess{
            lifecycle.topologyDocumentInitialized,
            lifecycle.topologyDocumentDirty,
            lifecycle.topologyDocumentStatus,
            lifecycle.currentLevelName,
            lifecycle.currentLevelPath,
            lifecycle.hasCurrentLevelPath,
            lifecycle.hasUnsavedChanges};
}

inline SectorEditorConstDocumentLifecycleAccess MakeSectorEditorDocumentLifecycleAccess(
        const SectorEditorDocumentLifecycleState& lifecycle)
{
    return SectorEditorConstDocumentLifecycleAccess{
            lifecycle.topologyDocumentInitialized,
            lifecycle.topologyDocumentDirty,
            lifecycle.topologyDocumentStatus,
            lifecycle.currentLevelName,
            lifecycle.currentLevelPath,
            lifecycle.hasCurrentLevelPath,
            lifecycle.hasUnsavedChanges};
}

inline SectorEditorConstDocumentLifecycleAccess MakeSectorEditorConstDocumentLifecycleAccess(
        SectorEditorDocumentLifecycleAccess lifecycle)
{
    return SectorEditorConstDocumentLifecycleAccess{
            lifecycle.topologyDocumentInitialized,
            lifecycle.topologyDocumentDirty,
            lifecycle.topologyDocumentStatus,
            lifecycle.currentLevelName,
            lifecycle.currentLevelPath,
            lifecycle.hasCurrentLevelPath,
            lifecycle.hasUnsavedChanges};
}

inline SectorEditorDocumentLifecycleAccess::operator SectorEditorConstDocumentLifecycleAccess() const
{
    return MakeSectorEditorConstDocumentLifecycleAccess(*this);
}

inline bool IsSectorEditorAuthoringDerivationCurrent(
        const SectorAuthoringDerivationResult& authoringDerivation,
        SectorEditorAuthoringDerivationState authoringDerivationState,
        bool authoringDerivedTopologyStale)
{
    return authoringDerivationState == SectorEditorAuthoringDerivationState::ValidCurrent
            && !authoringDerivedTopologyStale
            && authoringDerivation.success;
}

inline bool IsSectorEditorAuthoringDerivationCurrent(
        SectorEditorConstDerivationDocumentAccess derivation)
{
    return IsSectorEditorAuthoringDerivationCurrent(
            derivation.authoringDerivation,
            derivation.authoringDerivationState,
            derivation.authoringDerivedTopologyStale);
}

inline bool IsSectorEditorAuthoringDerivationCurrent(
        SectorEditorDerivationDocumentAccess derivation)
{
    return IsSectorEditorAuthoringDerivationCurrent(
            derivation.authoringDerivation,
            derivation.authoringDerivationState,
            derivation.authoringDerivedTopologyStale);
}

} // namespace game
