#version 330
uniform samplerCube sourceCube;
uniform sampler2D sourceFace;
uniform int faceIndex;
uniform int faceSize;
uniform int copyFace;
uniform float roughness;
out vec4 finalColor;
vec3 direction(vec2 uv) {
    vec2 p=uv*2.0-1.0;
    if(faceIndex==0)return normalize(vec3(1,-p.y,-p.x));
    if(faceIndex==1)return normalize(vec3(-1,-p.y,p.x));
    if(faceIndex==2)return normalize(vec3(p.x,1,p.y));
    if(faceIndex==3)return normalize(vec3(p.x,-1,-p.y));
    if(faceIndex==4)return normalize(vec3(p.x,-p.y,1));
    return normalize(vec3(-p.x,-p.y,-1));
}
float radical(uint bits) {
    bits=(bits<<16u)|(bits>>16u);
    bits=((bits&0x55555555u)<<1u)|((bits&0xAAAAAAAAu)>>1u);
    bits=((bits&0x33333333u)<<2u)|((bits&0xCCCCCCCCu)>>2u);
    bits=((bits&0x0F0F0F0Fu)<<4u)|((bits&0xF0F0F0F0u)>>4u);
    bits=((bits&0x00FF00FFu)<<8u)|((bits&0xFF00FF00u)>>8u);
    return float(bits)*2.3283064365386963e-10;
}
void main() {
    vec2 uv=gl_FragCoord.xy/float(faceSize);
    // Match the existing engine camera basis to OpenGL cubemap coordinates.
    if(copyFace!=0){finalColor=vec4(texture(sourceFace,vec2(1)-uv).rgb,1);return;}
    vec3 n=direction(uv);
    vec3 up=abs(n.z)<0.999?vec3(0,0,1):vec3(1,0,0);
    vec3 tangent=normalize(cross(up,n)),bitangent=cross(n,tangent);
    float a=roughness*roughness;
    vec3 sum=vec3(0);float weight=0;
    for(uint i=0u;i<64u;++i){
        float phi=6.28318530718*float(i)/64.0;
        float y=radical(i);
        float c=sqrt((1.0-y)/(1.0+(a*a-1.0)*y));
        float s=sqrt(max(0.0,1.0-c*c));
        vec3 h=tangent*cos(phi)*s+bitangent*sin(phi)*s+n*c;
        vec3 l=normalize(2.0*dot(n,h)*h-n);
        float w=max(dot(n,l),0.0);
        sum+=textureLod(sourceCube,l,0.0).rgb*w;weight+=w;
    }
    finalColor=vec4(sum/max(weight,0.00001),1);
}
