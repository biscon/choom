# Sector Authoring Graph Audit Plan

This is an audit-only runner-compatible plan.

Goal: gather focused reports before designing a larger implementation plan for a more Doom-editor-like authoring workflow.

Do not implement the authoring graph in this plan.

Do not add 3D doors in this plan.

3D doors may be mentioned only as a future constraint when relevant, especially around linedef/portal metadata.

```plan-state-json
{
  "plan_id": "sector_authoring_graph_audit_plan",
  "items": [
    {
      "id": "phase_01_closed_polygon_editor_assumptions",
      "title": "Audit closed-polygon assumptions in the current editor",
      "type": "phase",
      "status": "Completed",
      "parent": null
    },
    {
      "id": "phase_02_runtime_topology_consumers",
      "title": "Audit runtime consumers of already-valid SectorTopologyMap",
      "type": "phase",
      "status": "Completed",
      "parent": null
    },
    {
      "id": "phase_03_stable_ids_and_properties",
      "title": "Audit stable IDs and properties that must survive re-derivation",
      "type": "phase",
      "status": "Completed",
      "parent": null
    },
    {
      "id": "phase_04_authoring_graph_model",
      "title": "Audit candidate authoring graph model and Doom-like linedef concepts",
      "type": "phase",
      "status": "Completed",
      "parent": null
    },
    {
      "id": "phase_05_migration_path",
      "title": "Audit migration path for existing/current levels",
      "type": "phase",
      "status": "Completed",
      "parent": null
    }
  ]
}
```

## Global Instructions

Each phase is report-only.

For every phase:

* inspect the relevant code and docs
* do not change runtime/editor behavior
* do not refactor
* do not change serialization
* do not modify tests unless needed only to understand existing behavior, and prefer not to
* create or update only the requested audit report under `docs/topology_audits/`
* mark only the current plan item completed when the report is written
* keep findings concrete and code-grounded
* include file/function names where relevant
* include risks, unknowns, and recommended next questions
* do not invent a final architecture yet

Before writing reports, create the directory if needed:

```text
docs/topology_audits/
```

Use these report filenames:

```text
docs/topology_audits/01_closed_polygon_editor_assumptions.md
docs/topology_audits/02_runtime_topology_consumers.md
docs/topology_audits/03_stable_ids_and_properties.md
docs/topology_audits/04_authoring_graph_model.md
docs/topology_audits/05_migration_path.md
```

A future implementation plan may be derived from these reports later, but that is not part of this audit plan.

## Phase 1: Audit closed-polygon assumptions in the current editor

Selected item:

```text
phase_01_closed_polygon_editor_assumptions
```

Write:

```text
docs/topology_audits/01_closed_polygon_editor_assumptions.md
```

Question:

```text
Where does the current editor assume editable sectors are closed polygons?
```

Investigate:

* sector draw tool
* sector finalization
* validation during drawing
* selection/picking
* erase/delete behavior
* vertex move behavior
* shared vertex behavior
* inspector assumptions
* 2D rendering assumptions
* 3D preview launch assumptions
* JSON save/load expectations
* any code that assumes a sector is directly authored as an ordered closed loop

Search terms:

```text
SectorTopologyMap
SectorTopologySector
SectorTopologyVertex
SectorTopologyLinedef
SectorTopologySidedef
draw sector
close sector
pending
validation
self intersection
zero area
move vertex
erase
select sector
inspector
```

Report structure:

```text
# Closed-Polygon Editor Assumptions Audit

## Summary

## Editable Source Of Truth Today

## Closed Polygon Assumptions

## In-Progress Drawing Assumptions

## Selection / Inspector Assumptions

## Save / Load Assumptions

## What Would Break With Loose Lines

## Low-Risk Seams For Introducing An Authoring Graph

## Risks / Unknowns

## Recommended Follow-Up Questions
```

Completion criteria:

* report exists
* report identifies concrete files/functions
* report distinguishes current editable model from possible future authoring model
* plan item is marked Completed

## Phase 2: Audit runtime consumers of already-valid SectorTopologyMap

Selected item:

```text
phase_02_runtime_topology_consumers
```

Write:

```text
docs/topology_audits/02_runtime_topology_consumers.md
```

Question:

```text
Where does mesh/collision/light/preview consume SectorTopologyMap as already-valid runtime topology?
```

Investigate consumers such as:

* mesh generation
* triangulation
* wall/portal generation
* collision world building
* gameplay preview sector lookup
* lightmap baking
* static lights
* sky ceiling handling
* texture lookup/import/use
* 3D face selection and inspector mapping
* any validation expectations before runtime use

Search terms:

```text
BuildSector
SectorMeshPreview
SectorCollisionWorld
lightmap
Bake
earcut
triangulate
portal
upper
lower
ceilingSky
sidedef
wall part
face selection
hover
```

Report structure:

```text
# Runtime Topology Consumers Audit

## Summary

## Runtime Topology Contract Today

## Mesh Generation Consumers

## Collision Consumers

## Lightmap Consumers

## Gameplay Preview Consumers

## 3D Selection / Inspector Consumers

## Validation Assumptions

## Which Consumers Could Use A Derived Topology Unchanged

## Which Consumers Would Need Adaptation

## Risks / Unknowns

## Recommended Follow-Up Questions
```

Completion criteria:

* report exists
* report identifies concrete files/functions
* report explains what “already-valid topology” means today
* report identifies which systems should ideally keep consuming derived/runtime topology unchanged
* plan item is marked Completed

## Phase 3: Audit stable IDs and properties that must survive re-derivation

Selected item:

```text
phase_03_stable_ids_and_properties
```

Write:

```text
docs/topology_audits/03_stable_ids_and_properties.md
```

Question:

```text
What stable IDs/properties must survive if sectors are re-derived from a loose line graph?
```

Investigate:

* sector IDs
* vertex IDs
* linedef IDs
* sidedef IDs
* texture dictionary IDs
* per-sector floor/ceiling heights
* ambient brightness
* ceiling sky flag
* floor/ceiling textures
* wall/lower/upper sidedef texture overrides
* UV scale/offset
* lightmap settings/data
* static lights
* preview/player start settings
* selection state
* future feature constraints like door/action metadata on linedefs/portals

Search terms:

```text
id
nextId
texture
uvScale
uvOffset
ambient
ceilingSky
lightmap
static light
player start
preview
sidedef
linedef
sector
serialization
Json
```

Report structure:

```text
# Stable IDs And Property Survival Audit

## Summary

## Current ID Model

## Sector Properties To Preserve

## Linedef / Sidedef Properties To Preserve

## Vertex Properties To Preserve

## Texture And Asset References

## Lighting / Lightmap Data

## Preview / Player Start Data

## Selection / UI State

## Future Linedef Metadata Constraints

## Re-Derivation Risks

## Candidate Property Mapping Strategies

## Recommended Follow-Up Questions
```

Completion criteria:

* report exists
* report lists all important persisted properties found
* report identifies which properties are easy/hard to preserve through re-derivation
* report includes future door/linedef metadata only as a constraint, not as an implementation task
* plan item is marked Completed

## Phase 4: Audit candidate authoring graph model and Doom-like linedef concepts

Selected item:

```text
phase_04_authoring_graph_model
```

Write:

```text
docs/topology_audits/04_authoring_graph_model.md
```

Question:

```text
What should the authoring graph model contain, and how close can it stay to Doom linedef/vertex/sidedef concepts?
```

This is still an audit/design report, not implementation.

Investigate the current topology types and compare them to a possible split:

```text
Authoring graph:
  permissive source-of-truth for editor drawing

Derived topology:
  validated closed sectors/faces used by mesh/collision/light/preview
```

Consider whether the authoring graph should contain:

* vertices
* line segments / linedefs
* optional sidedef-like per-side metadata
* loose/dangling lines
* crossing lines before validation
* split/intersection handling
* tags/actions/future linedef specials
* editor-only invalid state
* derived-sector identity hints
* property anchors for re-derivation

Search terms:

```text
SectorTopologyMap
vertex
linedef
sidedef
sector
validation
intersection
split
portal
side
front
back
tag
```

Report structure:

```text
# Authoring Graph Model Audit

## Summary

## Current Topology Model

## Proposed Source / Derived Split

## Candidate Authoring Graph Elements

## Doom-Like Concepts That Map Well

## Doom-Like Concepts That Do Not Map Cleanly Yet

## Invalid Intermediate States To Support

## Validation And Derivation Responsibilities

## Property Anchoring Options

## Future Door / Linedef Special Constraints

## Risks / Unknowns

## Recommended Follow-Up Questions
```

Completion criteria:

* report exists
* report proposes candidate data responsibilities without implementing them
* report clearly separates authoring graph from derived topology
* report does not add 3D doors, but notes future door/linedef-special constraints
* plan item is marked Completed

## Phase 5: Audit migration path for existing/current levels

Selected item:

```text
phase_05_migration_path
```

Write:

```text
docs/topology_audits/05_migration_path.md
```

Question:

```text
What migration path lets old/current levels keep loading?
```

Investigate:

* current JSON schema
* default handling
* omitted defaults
* texture dictionary persistence
* sector/linedef/sidedef persistence
* lightmap persistence/versioning
* sample/working level loading
* fallback behavior
* tests around serialization
* whether an authoring graph can be added additively
* whether current topology can initially be converted into an authoring graph on load
* whether saved files should include both authoring graph and derived topology, or only authoring graph plus derivation

Search terms:

```text
Load
Save
Json
SectorTopology
ceilingSky
lightmap
version
working_level
sample
default
serialize
deserialize
```

Report structure:

```text
# Migration Path Audit

## Summary

## Current Serialized Format

## Compatibility Requirements

## Existing Default / Omitted-Field Behavior

## Current Level Files And Samples

## Additive Authoring Graph Options

## Conversion From Current Topology To Authoring Graph

## Save Format Options

## Backward Compatibility Risks

## Test Coverage Needed

## Recommended Migration Strategy Candidates

## Recommended Follow-Up Questions
```

Completion criteria:

* report exists
* report identifies current save/load files/functions
* report recommends candidate migration strategies without implementing them
* report calls out risks around persisted IDs/properties/lightmaps
* plan item is marked Completed

## Final Plan Completion Criteria

The audit plan is complete when:

* all five audit reports exist under `docs/topology_audits/`
* each report is concrete and code-grounded
* no runtime/editor behavior was changed
* no implementation plan was prematurely executed
* all five plan items are marked Completed
* `git diff --check` passes
* `git status --short` shows only expected plan/report changes

## Verification

For each phase, at minimum run:

```bash
git diff --check
git status --short
```

If code was accidentally changed, revert it unless the change is strictly limited to docs/plan status updates and requested audit reports.

No build is required for report-only phases unless code was changed accidentally and needs checking.
