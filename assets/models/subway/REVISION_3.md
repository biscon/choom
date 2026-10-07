# Revision 3 — vending machines

Both machines have been rebuilt through Blender MCP. Other approved meshes and reference placements were preserved.

- Cabinet shells and deep dark product wells replace the shallow original fronts.
- Each machine has four shelves, twelve labelled selection lanes, dispensing spirals and twenty-four products stocked two deep.
- Snack packets have folded gussets, shaped bodies and pinched crimped seals. Four fictional print designs come from a shared imagegen atlas.
- Drinks include cola/citrus pull-tab cans and capped water/tea bottles, with wrapped labels, formed shoulders, rim details and cap fluting.
- Payment hardware includes labelled keypad buttons, credit display, coin slot/return, bill mouth and recessed collection flap. Added hinges, key lock, toe kick, adjustable feet, intake louvers and rear service panel.
- Weathered cabinet paint and steel reuse the train's shared PBR maps. Only two new 1024² atlases were added: VendingProducts.png and VendingControls.png. Both machines use the same external image paths.

Vending_Snacks: 13,824 triangles. Vending_Drinks: 27,216 triangles. Both have bottom-centred origins, 0.94 m cabinet width, 0.976 m overall width including vent plates, 1.91 m height and 0.931 m overall depth. Front remains glTF +Z, up +Y. Replace the .gltf and matching .bin and copy the shared textures folder.

All materials are opaque. The dark display backing and visible stock do not require glass transmission. Header emission remains 0.65; credit display 0.35 and internal light strip 1.4. Actual engine lighting is separate.

Validation: no detected coplanar triangle overlaps; all components form one contact group per asset; closed shells, nonzero areas, UVs and unit normals/tangents checked; all 24 exports reimported successfully. Other 22 assets retain their prior triangle counts and bounds. Front, rear, elevated product closeups and low-angle foot/underside views were inspected. Preview lighting is Cycles/AgX, not engine ACES validation.

Source: subway_reference.blend. Rebuild changes live in scripts/vending_v3.py, vending_products.py, vending_materials.py and make_vending_controls.py. scripts/rebuild_vending.py is the targeted live-namespace rebuild, preserving other meshes and reference instances.

Final review images: renders/19_vending_revision3.png, 20_snack_detail.png, 21_drink_detail.png, 22_vending_rear.png, 23_vending_low.png. Earlier station/furniture renders containing vending machines have also been refreshed.

Texture provenance: built-in imagegen; source and exact prompt are reference/vending_product_art_source.png and reference/vending_revision3_prompt.txt. Original pre-revision scene and builders are retained in reference/pre_revision_3/.
