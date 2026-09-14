#ifndef SECTOR_MACRO_VARIATION_GLSL
#define SECTOR_MACRO_VARIATION_GLSL

uniform sampler2D macroTexture;
uniform int hasMacro;
// Repeat size in runtime metres, darkening, signed roughness change.
uniform vec3 macroParameters;

float SectorMacroMask(vec3 worldPosition, vec3 geometricNormal)
{
    if (hasMacro == 0) return 0.0;
    vec3 p = worldPosition / max(macroParameters.x, 0.1);
    vec3 weights = abs(geometricNormal);
    weights *= weights;
    weights *= weights;
    weights /= max(weights.x + weights.y + weights.z, 0.00001);
    // Both wall projections keep image V vertical; floor projection uses X/Z.
    return dot(weights, vec3(texture(macroTexture, p.zy).r,
                             texture(macroTexture, p.xz).r,
                             texture(macroTexture, p.xy).r));
}
#endif
