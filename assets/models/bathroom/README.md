# Bathroom glTF assets

22 individual assets. Keep each `.gltf` and matching `.bin` together, with the shared `textures/` folder. Units: metres; +Y up, +Z front. See `../../README.md` and `../../manifest.json` for placement and material details.

Mirrors have a rough mottled finish designed for ordinary reflection probes. Fluorescent tubes use an emission texture and `KHR_materials_emissive_strength` = 2. Base color/emission are sRGB; OpenGL Y+ normals and ORM are linear. ORM: R occlusion, G roughness, B metallic.
