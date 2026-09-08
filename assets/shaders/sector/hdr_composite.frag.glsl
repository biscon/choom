#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D sceneColor;
uniform sampler2D sourceColor;
uniform int compositeMode;
float safeRadianceChannel(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? 65504.0 : 0.0;
    return min(max(value,0.0),65504.0);
}
vec3 safeRadiance(vec3 value) {
    return vec3(safeRadianceChannel(value.r),safeRadianceChannel(value.g),
            safeRadianceChannel(value.b));
}
float safeAlpha(float value) {
    return (isnan(value)||isinf(value))?1.0:clamp(value,0.0,1.0);
}
void main() {
    vec4 source=texture(sourceColor,fragUv);
    if (compositeMode == 1) {
        vec4 scene=texture(sceneColor,fragUv);
        float coverage=isnan(source.a)||isinf(source.a)?0.0:clamp(source.a,0.0,1.0);
        finalColor=vec4(safeRadiance(safeRadiance(scene.rgb)*(1.0-coverage)
                +safeRadiance(source.rgb)),safeAlpha(scene.a));
        return;
    }
    finalColor=vec4(clamp(safeRadiance(source.rgb),vec3(0.0),vec3(65504.0)),safeAlpha(source.a));
}
