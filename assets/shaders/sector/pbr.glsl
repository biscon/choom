#ifndef SECTOR_PBR_GLSL
#define SECTOR_PBR_GLSL

// Bounded normal-variance filtering (Tokuyoshi/Kaplanyan; also used by Filament).
// Evaluate once per fragment, before discard or divergent lighting branches.
// enabled must be uniform across the draw. Roughness here is perceptual: GGX
// uses alpha = roughness^2, so the filter adds variance to roughness^4.
float FilterSpecularRoughness(float roughness, vec3 worldNormal, bool enabled)
{
    if (!enabled) return roughness;
    vec3 normalDx = dFdx(worldNormal);
    vec3 normalDy = dFdy(worldNormal);
    const float VarianceScale = 0.15;
    const float MaxKernelContribution = 0.2;
    float variance = VarianceScale
            * (dot(normalDx, normalDx) + dot(normalDy, normalDy));
    float alpha = roughness * roughness;
    float kernel = min(2.0 * variance, MaxKernelContribution);
    return sqrt(sqrt(clamp(alpha * alpha + kernel, 0.0, 1.0)));
}

float DistributionGgx(vec3 normal, vec3 halfway, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(normal, halfway), 0.0);
    float denominator = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denominator * denominator, 0.000001);
}

float GeometrySchlickGgx(float ndotv, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return ndotv / max(ndotv * (1.0 - k) + k, 0.000001);
}

vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0)
            * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec2 EnvironmentBrdfApprox(float roughness, float ndotv)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}


float GeometrySmith(
        vec3 normal,
        vec3 viewDirection,
        vec3 lightDirection,
        float roughness)
{
    return GeometrySchlickGgx(
            max(dot(normal, viewDirection), 0.0), roughness)
            * GeometrySchlickGgx(
                    max(dot(normal, lightDirection), 0.0), roughness);
}

#endif
