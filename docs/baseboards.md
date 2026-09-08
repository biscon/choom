# Procedural baseboards

Select an Authoring Line in the 2D editor. Each Front Side and Back Side section
has an independent **Baseboard** checkbox. Enable it to reveal Height, Thickness,
and Material. Default clears the material assignment and uses the engine default.

Dimensions use existing authoring units: 8 units = 1 meter. Defaults are height
1.0 (12.5 cm) and thickness 0.14 (1.75 cm). Disabling trim preserves its settings.
Existing documents load with trim disabled. Saving adds only optional non-default
`baseboard` fields to authoring line sides; no format migration is required.

Boards follow solid walls at the owning sector's floor. Lower wall strips can
receive trim, clipped to their available height. Open portals and middle textures
do not receive trim, so boards never cross a floor-level doorway. Boards on
opposite sides are independent. Geometry is built during explicit rebuilds.

Adjacent boards use shared miter cuts and omit internal meeting faces. Straight
joins support different thicknesses; height changes expose only the uncovered cap.
Long exterior miters become bevels. Acute interior corners that cannot fit the
requested thickness shorten and cap the affected spans; exhausted short spans are
omitted with a console warning. Reducing thickness or splitting the line can resolve
these diagnostics. No collision geometry, sector lookup, physics, or camera behavior
is changed.

Trim uses the existing PBR mesh pipeline, structural-box texture density/orientation,
lightmap charts, baked occlusion, dynamic-shadow eligibility, and sector visibility.
Authoring edits invalidate the 2D cache and derived preview geometry. Enabled trim
settings and materials affect the lightmap source hash; disabled settings and purely
visual preview/sky settings do not. Re-bake after enabling or changing trim.

Material IDs: `baseboard_wood`, `baseboard_metal`, `baseboard_painted_white`.
Each has an albedo, OpenGL normal map, and roughness map under
`assets/images/baseboards/`. That directory's README records the imagegen prompts.

Automated fixtures cover serialization, authoring projection/splitting, invalidation,
join overlap, openings, lightmap hashing/layout, inspector layout, and unchanged
collision edges. Interactive editor verification remains with the user: inspect
corners and texture repeats, bake lighting, and confirm the final inspector controls
are accessible at maximum scroll at your UI scale.
