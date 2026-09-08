# Baseboard texture generation

Generated with the built-in imagegen tool. Files are used by `assets/materials/materials.json`.
Albedo maps are base color; `_normal` maps use OpenGL tangent-space encoding; `_roughness` maps are scalar roughness. No baked lighting is intended. Grain/brushing runs along U.

## baseboard_wood.png

Use case: photorealistic-natural. Asset type: seamless PBR game material albedo texture, 1024x1024 square. Generate one flat orthographic texture of warm medium brown oak wood for a narrow baseboard/skirting board. Fine subtle continuous natural wood grain running horizontally, no plank divisions, no board edges or profile, no knots larger than 1% of image. Entire image is the wood surface. Seamlessly tileable on BOTH axes, uniformly lit diffuse base color only, no shading, shadows, specular highlights, perspective, text, border, or watermark. Small-scale grain appropriate to a 2 meter texture repeat. Output a project texture file.

## baseboard_wood_normal.png

Use case: precise-object-edit. Asset type: tangent-space OpenGL normal map companion for the supplied seamless wood albedo game texture. Convert the reference into ONLY its matching subtle microrelief normal map. Preserve identical wood grain positions, horizontal direction, scale, square dimensions, framing and seamless repeat. Normal map encoding: neutral flat surface RGB 128,128,255; X in red, positive Y in green, positive Z blue. Predominantly pale periwinkle blue with fine subtle pink/cyan slopes around wood pores, shallow relief, no brown albedo color, no illumination, no text, no border. Do not invent or move grain features. One normal map image only.

## baseboard_wood_roughness.png

Use case: precise-object-edit. Asset type: scalar roughness map for supplied seamless oak albedo texture. Produce one grayscale-only roughness companion image preserving the exact horizontal grain locations, framing and texture scale. Satin finished oak: mean gray 145 of 255, gentle range 120-170, fine darker smooth grain ridges and subtly lighter rough pores. Roughness not height: no harsh black lines, no baked lighting. Seamlessly tileable BOTH axes. Keep original square dimensions. No color, border, text or watermark. Output only the roughness map.

## baseboard_metal.png

Use case: photorealistic-natural. Asset type: seamless PBR base-color game material texture. One square 1024x1024 flat orthographic texture of clean brushed satin stainless steel for architectural baseboard trim. Neutral light silver-gray metal base color, extremely fine horizontal brushing running continuously across the entire surface. Seamlessly tileable BOTH axes. Uniform color and diffuse flat illumination only: absolutely no directional highlights, reflections, gradient, shadows, perspective, borders, screws, plank divisions, text or watermark. Entire image is material. This is a metallic-workflow base color map; actual reflections are rendered by the engine. Subtle fine detail suitable for a 2 meter texture repeat.

## baseboard_metal_normal.png

Use case: precise-object-edit. Asset type: tangent-space OpenGL normal map companion for the supplied seamless brushed steel albedo. Output ONLY a very subtle normal map of the identical microscopic horizontal brushing. Preserve exact feature locations, square dimensions, framing, texture scale, and seamless repeat on BOTH axes. RGB neutral flat surface 128,128,255; X red, positive Y green, positive Z blue. Keep channels very near flat blue, extremely shallow fine brush grooves; no albedo gray, lighting, gradient, reflections, text or border. One normal map image.

## baseboard_metal_roughness.png

Use case: precise-object-edit. Asset type: scalar roughness companion for supplied seamless brushed stainless steel base-color texture. Output one grayscale roughness map with identical microscopic horizontal brush positions, scale, framing and square dimensions. Satin brushed metal roughness: mean gray 100 of 255 with delicate variations between 80 and 120; no coarse contrasts. Uniform appearance seamlessly tileable BOTH axes. No lighting, reflections, gradients, color, border, text, or watermark. Output roughness only, NOT a height or albedo image.

## baseboard_painted_white.png

Use case: photorealistic-natural. Asset type: seamless PBR game albedo material texture. One square 1024x1024 flat orthographic texture of off-white satin-painted wooden architectural skirting/baseboard. Warm white around RGB 224,222,215, fine delicate horizontal paintbrush microtexture with very subtle wood grain showing beneath paint. Clean maintained finish, no dirt patches or peeling. Entire image is the surface; no board outlines, molding, edges or divisions. Seamlessly tileable BOTH axes, absolutely uniform flat diffuse lighting, no shadows, highlights, shading gradient, perspective, text, border or watermark. Microtexture appropriate for a 2 meter repeat, not a closeup of large brush ridges.

## baseboard_painted_white_normal.png

Use case: precise-object-edit. Asset type: tangent-space OpenGL normal map companion for supplied seamless off-white paint texture. Output only a very shallow normal map matching the exact fine horizontal paintbrush and wood microtexture locations. Keep the same square dimensions, framing, scale and seamless BOTH-axis repetition. Encode flat normal RGB 128,128,255, X red, positive Y green, positive Z blue. Mostly uniform pale blue with very delicate small cyan/pink variations, smoother than bare wood. No off-white albedo color, no shadows, highlights, perspective, text, border or watermark.

## baseboard_painted_white_roughness.png

Use case: precise-object-edit. Asset type: scalar roughness companion texture for the supplied seamless off-white satin paint albedo. Output one grayscale-only map preserving matching fine horizontal microtexture positions, framing, texture scale and square dimensions. Satin painted wood: mean gray 135 of 255, restrained range 120-150, subtle rougher fine brush ridges. Seamlessly tileable BOTH axes. Uniform appearance, no baked lighting, shadows, directional gradient, highlights, color, text, border or watermark. This is roughness data, not a height or albedo map.


