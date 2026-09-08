#version 330
in vec4 fragColor;
out vec4 finalColor;
uniform float radianceStrength;
vec3 srgbToLinear(vec3 c) {
    vec3 low=c/12.92;
    vec3 high=pow((c+0.055)/1.055,vec3(2.4));
    return mix(high,low,lessThanEqual(c,vec3(0.04045)));
}
float storeFiniteHalfChannel(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? 65504.0 : 0.0;
    return min(max(value,0.0),65504.0);
}
vec3 storeFiniteHalfRadiance(vec3 value) {
    return vec3(storeFiniteHalfChannel(value.r),storeFiniteHalfChannel(value.g),
            storeFiniteHalfChannel(value.b));
}
void main() {
    float coverage=isnan(fragColor.a)||isinf(fragColor.a)?0.0:clamp(fragColor.a,0.0,1.0);
    vec3 radiance=srgbToLinear(clamp(fragColor.rgb,0.0,1.0))*max(radianceStrength,0.0)*coverage;
    finalColor=vec4(storeFiniteHalfRadiance(radiance),0.0);
}
