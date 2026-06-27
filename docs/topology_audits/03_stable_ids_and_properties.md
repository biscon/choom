# Stable IDs And Property Survival Audit

## Summary

If editable sectors are later re-derived from a loose line graph, the hard part is not just reconstructing valid loops. The editor must also preserve stable positive IDs and all persisted authoring properties currently stored on `SectorTopologyMap`, because IDs are used for selection, serialization order, generated surface identity, and the lightmap source hash.

The current schema is topology v2 / linedef-based in `sources/sector_demo/SectorTopologyTypes.h` and `sources/sector_demo/SectorTopologyMap.h`. JSON persistence in `sources/sector_demo/SectorTopologySerialization.cpp` writes vertices, linedefs, sidedefs, sectors, static lights, texture definitions, lightmap settings, preview settings, sky settings, directional light, and baked lightmap metadata. `ComputeSectorLightmapSourceHash()` in `sources/sector_demo/SectorLightmap.cpp` also hashes most geometry, material, static light, directional light, and ID fields, so re-derivation that changes IDs can make an otherwise visually equivalent bake stale.

## Current ID Model

IDs are stable positive integers. `IsValidSectorTopologyId()` in `SectorTopologyMap.cpp` accepts `id > 0`, and `AllocateSectorTopologyVertexId()`, `AllocateSectorTopologyLineDefId()`, `AllocateSectorTopologySideDefId()`, `AllocateSectorTopologySectorId()`, and `AllocateSectorTopologyStaticLightId()` allocate `max(existing id) + 1`.

`SectorTopologyMap` stores separate ID spaces for vertices, linedefs, sidedefs, sectors, and static lights. Texture IDs are string keys in `texturesById`, not integer IDs. `BuildSectorTopologyIndexes()` builds transient lookup tables by ID and intentionally keeps duplicate IDs so malformed maps can be diagnosed.

Serialization sorts ID-bearing records by ID before writing. `SerializeMap()` uses `SortedById()` for vertices, linedefs, sidedefs, sectors, and static lights. This makes ID stability visible in file diffs, not just in runtime behavior.

Current edit helpers already establish some property-transfer precedents:

* `CreateSectorTopologyPolygon()` reuses an existing vertex at the same coordinate and may reuse an existing physical linedef in the same or reversed direction.
* `InsertSectorTopologyPolygon()` copies the parent sector into the child with a new sector ID and name through `CopySectorForInsertedChild()`.
* `SplitSectorTopologyLineDefAtPoint()` creates a new vertex and two new linedefs, duplicates the original front/back sidedefs, copies linedef flags, and removes the original linedef/sidedefs.
* `DissolveSectorTopologyVertex()` creates a replacement linedef and duplicates compatible sidedef data onto it.
* `CutSectorTopologySectorBetweenBoundaryPoints()` clones the original sector to create the new sector, reassigns existing boundary sidedefs to the result sectors, and creates the new cut linedef/sidedefs from sector defaults.

These helpers preserve properties locally, but they do not solve whole-map re-derivation identity.

## Sector Properties To Preserve

`SectorTopologySector` in `SectorTopologyTypes.h` persists:

* `id` and `name`
* `floorZ` and `ceilingZ`
* `floorTextureId` and `ceilingTextureId`
* `ceilingSky`
* `floorUv` and `ceilingUv`
* `floorDecal` and `ceilingDecal`
* `ambientColor` and `ambientIntensity`
* `defaultWall`, `defaultLower`, and `defaultUpper`

The flat UV settings contain `scale` and `offset`; decals contain `textureId`, `uv`, `opacity`, `emissive`, `tint`, and `bloomIntensity`. `ceilingSky` is optional on load and omitted on save when false, but it changes generated geometry and is hash-sensitive.

Easy to preserve through re-derivation: properties for sectors that can be matched one-to-one by unchanged boundary identity or by a durable authoring-sector anchor.

Hard to preserve: properties after a loose graph edit splits one region into multiple regions, merges two regions, deletes a boundary, or changes a boundary enough that polygon matching is ambiguous. Sector names, heights, ambient settings, flat materials, decals, default sidedef materials, and `ceilingSky` all need explicit conflict rules.

## Linedef / Sidedef Properties To Preserve

`SectorTopologyLineDef` persists:

* `id`
* `startVertexId` and `endVertexId`
* `frontSideDefId` and `backSideDefId`
* `flags.blocksPlayer`

`SectorTopologySideDef` persists:

* `id`
* `lineDefId`
* `side`
* `sectorId`
* `wall`, `lower`, `upper`, and optional `middle` part settings

Each wall part stores `textureId`, UV `scale`/`offset`, and optional decal settings. Middle textures are persisted on sidedefs and can affect preview/collision through linedef-side data; they currently receive baked light but are not alpha-aware shadow occluders.

Easy to preserve: a derived linedef that maps to the same authoring segment endpoints in the same orientation can keep `id`, `flags`, and side assignments. Sidedef material preservation is also straightforward when the same sector remains on the same directed side.

Hard to preserve: splitting an authoring line into multiple derived linedefs, merging collinear linedefs, resolving intersections, or flipping line direction. Existing helpers duplicate sidedef settings on splits, but whole-map derivation needs deterministic rules for whether one original linedef ID survives and how new child linedefs inherit wall, lower, upper, middle, decal, UV, and `blocksPlayer` metadata. Portal identity is especially sensitive because both front/back sidedefs and their sector IDs define the portal relation.

## Vertex Properties To Preserve

`SectorTopologyVertex` persists only:

* `id`
* exact integer `x`
* exact integer `y`

Coordinates use `SectorCoord` with `SectorCoordSubdivisions == 16`. Re-derivation should preserve vertex IDs for unchanged coordinates and for authoring vertices that remain logically the same. It may need new IDs for inserted intersection/split vertices. It should avoid recycling IDs for a different logical point because selections, generated surfaces, and lightmap hashes can all observe that change.

Easy to preserve: unchanged authoring vertices and exact-coordinate reuse.

Hard to preserve: auto-split intersections, merged vertices, and points created or removed by graph normalization. Coordinate-only matching is useful but not sufficient if two vertices are temporarily coincident or if an edit intentionally replaces one vertex with another.

## Texture And Asset References

Texture definitions are stored in `SectorTopologyMap::texturesById` as string-keyed `SectorTextureDefinition` records with:

* `id`
* `path`
* `filter`

`SerializeMap()` writes them under root `textures`, sorted by texture ID. Materials throughout sectors, sidedefs, decals, and `skySettings.textureId` refer to these string IDs. Missing texture IDs must remain non-crashing at render time, but persistence should keep the references exactly.

`ComputeSectorLightmapSourceHash()` includes only texture definitions referenced by lightmap-relevant material fields: sidedef wall/lower/upper/middle texture IDs and sector floor/ceiling/default wall/default lower/default upper texture IDs. It does not include decal texture IDs and does not include `skySettings.textureId`.

Easy to preserve: texture registry and string references can survive re-derivation unchanged if material anchors survive.

Hard to preserve: wall material references when one authored side maps to multiple derived sidedefs or when a derived side changes which sector owns it.

## Lighting / Lightmap Data

Static lights are persisted as `SectorTopologyStaticPointLight` records with:

* `id`
* authoring-space `position`
* `color`
* `intensity`
* `radius`
* `sourceRadius`

`SectorLightmapBakeSettings` persists ambient occlusion radius/strength and indirect bounce radius/strength. `SectorTopologyDirectionalLightSettings` persists enabled state, direction-to-light, color, and intensity when non-default. `SectorLightmapMetadata` persists baked lightmap `path`, `width`, `height`, and `sourceHash` when complete.

`ComputeSectorLightmapSourceHash()` includes bake settings, directional light settings, `SectorCoordSubdivisions`, referenced texture definitions, vertices including IDs, linedefs including IDs and endpoint/sidedef IDs, sidedefs including IDs and material wall parts, sectors including IDs, heights, `ceilingSky`, floor/ceiling material and UV, ambient settings, default wall parts, and static lights including IDs and world-space light parameters.

The hash currently excludes preview settings, sky visual settings, baked lightmap metadata itself, sector names, and decal fields. Re-derivation that changes stable topology IDs will stale the bake even if the generated shapes are equivalent.

Easy to preserve: static lights and lightmap settings are map-level or independently ID-bearing and do not need to be re-derived from sector loops.

Hard to preserve: baked lightmap validity across re-derivation. To keep an existing bake non-stale, the derived topology IDs and all hash-sensitive fields must remain exactly stable, not just visually similar.

## Preview / Player Start Data

There is no persisted explicit player-start object in `SectorTopologyMap`. Persisted preview data is `SectorPreviewSettings`:

* `walkSpeed`
* `runSpeed`
* `mouseSensitivity`
* `eyeHeight`
* `gravity`
* `playerRadius`
* `playerHeight`
* `stepHeight`
* `jumpHeight`
* `headBobStrength`
* `headBobFrequency`

`SectorEditorState` also holds transient preview/controller state such as `freeflyController`, `fpsControllerConfig`, `fpsControllerState`, `previewControlMode`, `lastPreviewPose`, collision results, and preview UI visibility. `SectorEditorPreviewActions.cpp` resets or carries this state while entering/rebuilding preview, but it is not part of JSON persistence.

Easy to preserve: persisted preview settings are map-level and independent of derived sector identity.

Hard to preserve: if a future player-start object is added, it will need a stable anchor independent of transient preview camera pose, likely a map-level thing with position and optional sector hint.

## Selection / UI State

Selection is transient editor state in `SectorEditorState`:

* `topologySelectionKind`
* selected sector, vertex, sidedef, linedef, side kind, wall part, material layer, and light IDs
* hovered light/vertex and inspected vertex IDs
* 3D selected surface references and material edit targets
* pending draw, split, merge, cut, vertex drag, and light drag states

The 2D render cache also stores cached sector, linedef, vertex, and light draw records by stable IDs. None of this is serialized as map data, but ID preservation affects whether selection and hover state can survive an in-editor re-derivation without being cleared.

Easy to preserve: selection for unchanged IDs.

Hard to preserve: selected sidedef/wall part after line splitting, selected sector after face split/merge, and selected 3D surface refs after derived surface IDs change. A re-derivation pass should either map old selections to new IDs or deliberately clear stale selections.

## Future Linedef Metadata Constraints

The current future-facing linedef metadata is `SectorTopologyLineDefFlags::blocksPlayer`, edited through `SetPortalBlocksPlayer()` in `SectorEditorTopologyActions.cpp`. Future door/action/special metadata would naturally live on linedefs, portals, or directed sides. This audit does not add doors or specials, but it means re-derivation cannot treat linedefs as disposable geometry if authored line metadata is expected to survive.

Door/action metadata would likely need to remain anchored to authoring line identity, then be projected onto one or more derived linedefs. Derived portal sidedefs would need conflict rules when one authoring line borders multiple derived sectors or when a line is split by intersections.

## Re-Derivation Risks

Changing IDs is behaviorally visible. It changes save order, transient selections, generated surface refs, and lightmap source hashes.

Sector property loss is likely when one loose graph edit changes face topology. A new region cannot safely guess heights, sky, ambient, floor/ceiling materials, default wall materials, or decals without explicit inheritance rules.

Sidedef property loss is likely around splits, merges, and line direction changes. Wall/lower/upper/middle material overrides are concrete sidedef data today, not just sector defaults.

Lightmap validity is fragile. The current source hash includes topology IDs themselves, so preserving geometry alone will not preserve `bakedLightmap.sourceHash`.

Texture references are string-based and easy to copy, but a material anchor must still identify which derived sector or directed side receives the texture.

Static lights are independent of sector derivation, but if later tools constrain lights to sectors, their sector hints would need stable mapping.

## Candidate Property Mapping Strategies

Use durable authoring IDs as the primary anchors. Keep authoring vertices, lines, directed sides, and face/property anchors distinct from derived topology IDs so derived objects can be regenerated while properties remain attached to the source graph.

Maintain an explicit derivation map from authoring vertices/lines/sides/faces to derived vertices/linedefs/sidedefs/sectors. This map can carry selection remaps and can decide whether to preserve, duplicate, or allocate new derived IDs.

For vertices, prefer exact authoring vertex identity first, then exact-coordinate matching only as a fallback for legacy maps. Inserted intersection vertices should get deterministic new IDs and should not steal an unrelated old vertex ID.

For linedefs, preserve the old ID only when the derived segment still represents the same unsplit physical segment. When split, choose a deterministic survivor segment for the old ID and allocate new IDs for the rest, or accept that the whole split invalidates lightmaps and selections.

For sidedefs, map by authoring line side plus owning sector/face. When one side splits into many derived sides, duplicate material properties the way `SplitSectorTopologyLineDefAtPoint()` does today.

For sectors, map by explicit face/property anchor when possible. For ambiguous split/merge cases, prefer conservative rules: keep the selected/original region's ID where unambiguous, clone sector properties for new regions created by a cut, and require user choice or defaults for merges.

For lightmaps, decide whether ID stability is a hard requirement for non-stale bakes. If not, re-derivation can intentionally stale the bake whenever topology is regenerated. If yes, the derivation layer must preserve hash-sensitive IDs exactly for unchanged logical topology.

## Recommended Follow-Up Questions

Should derived topology IDs be persisted, or should persisted IDs move to authoring graph objects with derived IDs treated as cache output?

What is the intended inheritance rule when a loose-line edit splits one sector into two derived sectors outside the existing explicit cut tool?

When a derived linedef is split by intersections, which child, if any, keeps the original linedef ID and future action metadata?

Should lightmap source hash continue to include topology IDs after an authoring/derived split, or should it hash only generated geometry and bake-relevant properties?

Should future player start data be a map-level persisted object before authoring graph migration, so preview/runtime start behavior has a stable property anchor?
