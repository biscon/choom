#ifndef SECTOR_REFLECTION_SAMPLING_GLSL
#define SECTOR_REFLECTION_SAMPLING_GLSL

uniform samplerCube rpCurrent0;
uniform samplerCube rpPrevious0;
uniform samplerCube rpCurrent1;
uniform samplerCube rpPrevious1;
uniform vec3 rpCapture[2];
uniform vec3 rpCenter[2];
uniform vec3 rpExtents[2];
// yaw, intensity, maxLod, box projection
uniform vec4 rpParameters[2];
uniform vec2 rpTransition;
uniform vec2 rpWidths;
uniform vec4 rpPlane;
uniform vec4 rpAperture;
uniform vec2 rpHeights;
uniform int rpMode;
vec3 ReflectionLocal(vec3 p, float yaw) {
    float c=cos(yaw), s=sin(yaw);
    return vec3(p.x*c-p.z*s,p.y,p.x*s+p.z*c);
}
vec3 ReflectionDirection(int i, vec3 position, vec3 direction) {
    if (rpParameters[i].w < 0.5) return direction;
    vec3 origin=ReflectionLocal(position-rpCenter[i],-rpParameters[i].x);
    vec3 ray=ReflectionLocal(direction,-rpParameters[i].x);
    vec3 safeRay=mix(vec3(-1),vec3(1),step(vec3(0),ray))*max(abs(ray),vec3(0.00001));
    vec3 plane=mix(-rpExtents[i],rpExtents[i],step(vec3(0),ray));
    vec3 distance=(plane-origin)/safeRay;
    vec3 hit=origin+ray*max(min(distance.x,min(distance.y,distance.z)),0.0);
    vec3 lookup=hit-ReflectionLocal(rpCapture[i]-rpCenter[i],-rpParameters[i].x);
    if (dot(lookup,lookup)<0.000001) return direction;
    return normalize(ReflectionLocal(lookup,rpParameters[i].x));
}
float ReflectionWeight(int i, vec3 position) {
    if (rpParameters[i].y<=0.0) return 0.0;
    if (rpParameters[i].w<0.5) return 0.0001;
    vec3 local=abs(ReflectionLocal(position-rpCenter[i],-rpParameters[i].x));
    vec3 edge=rpExtents[i]-local;
    float d=min(edge.x,min(edge.y,edge.z));
    return rpWidths[i]>0.0 ? clamp(d/rpWidths[i],0.0,1.0) : step(0.0,d);
}
vec3 SampleSectorEnvironment(vec3 position, vec3 direction, float roughness) {
    if (rpMode==0) return vec3(0);
    vec3 d0=ReflectionDirection(0,position,direction);
    float l0=roughness*rpParameters[0].z;
    vec3 a=textureLod(rpCurrent0,d0,l0).rgb;
    if (rpTransition.x<1.0) a=mix(textureLod(rpPrevious0,d0,l0).rgb,a,rpTransition.x);
    a*=rpParameters[0].y;
    if (rpMode==1) return a;
    vec3 d1=ReflectionDirection(1,position,direction);
    float l1=roughness*rpParameters[1].z;
    vec3 b=textureLod(rpCurrent1,d1,l1).rgb;
    if (rpTransition.y<1.0) b=mix(textureLod(rpPrevious1,d1,l1).rgb,b,rpTransition.y);
    b*=rpParameters[1].y;
    float weight;
    if (rpMode==3) {
        float along=dot(position.xz,rpAperture.xy)-rpAperture.z;
        if (along<0.0 || along>rpAperture.w || position.y<rpHeights.x || position.y>rpHeights.y) return a;
        float d=dot(rpPlane.xyz,position)+rpPlane.w;
        weight=smoothstep(-max(rpWidths.x,0.00001),max(rpWidths.y,0.00001),d);
    } else {
        float wa=ReflectionWeight(0,position), wb=ReflectionWeight(1,position);
        weight=wa+wb>0.00001 ? wb/(wa+wb) : 0.0;
    }
    return mix(a,b,weight);
}

#endif
