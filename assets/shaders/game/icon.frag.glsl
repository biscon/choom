#version 330
in vec2 fragTexCoord;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;
void main() {
    vec4 base = texture(texture0, fragTexCoord)*colDiffuse;
    if (base.a <= 0.001) discard;
    vec3 n = normalize(fragNormal);
    float key = max(dot(n, normalize(vec3(-0.45, 0.78, 0.42))), 0.0);
    float fill = max(dot(n, normalize(vec3(0.70, 0.30, -0.64))), 0.0);
    float rim = pow(1.0-max(dot(n, normalize(vec3(-0.53, -0.40, -0.75))), 0.0), 3.0);
    vec3 light = vec3(0.20) + vec3(0.70)*key + vec3(0.24,0.30,0.42)*fill
            + vec3(0.20,0.26,0.34)*rim;
    finalColor = vec4(base.rgb*light, base.a);
}
