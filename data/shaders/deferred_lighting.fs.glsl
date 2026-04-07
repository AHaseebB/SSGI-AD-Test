#version 460 core

layout (location = 0) out vec4 oPixel;

layout (binding = 0) uniform sampler2D uGAlbedo;
layout (binding = 1) uniform sampler2D uGNormal;
layout (binding = 2) uniform sampler2D uGMaterial;
layout (binding = 3) uniform sampler2D uGDepth;

layout (location = 0) uniform vec2 uScreenSize;
layout (location = 1) uniform mat4 uInvProjection;
layout (location = 2) uniform mat4 uInvView;

layout (location = 3) uniform vec3 uLightDirection;
layout (location = 4) uniform vec3 uLightColor;
layout (location = 5) uniform float uAmbientStrength;
layout (location = 6) uniform vec3 uCameraPosition;
layout (location = 7) uniform int uDebugView;

vec3 ReconstructWorldPosition(vec2 uv, float depth)
{
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = uInvProjection * clip;
    viewPos /= viewPos.w;
    vec4 worldPos = uInvView * viewPos;
    return worldPos.xyz;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / uScreenSize;

    vec3 albedo = texture(uGAlbedo, uv).rgb;
    vec3 normal = texture(uGNormal, uv).rgb * 2.0 - 1.0;
    vec2 material = texture(uGMaterial, uv).rg;
    float depth = texture(uGDepth, uv).r;

    if (uDebugView == 1)
    {
        oPixel = vec4(albedo, 1.0);
        return;
    }
    if (uDebugView == 2)
    {
        oPixel = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }
    if (uDebugView == 3)
    {
        oPixel = vec4(material.r, material.g, 0.0, 1.0);
        return;
    }
    if (uDebugView == 4)
    {
        oPixel = vec4(vec3(depth), 1.0);
        return;
    }

    vec3 worldPos = ReconstructWorldPosition(uv, depth);

    float roughness = material.r;
    float metallic = material.g;

    vec3 N = normalize(normal);
    vec3 L = normalize(-uLightDirection);
    vec3 V = normalize(uCameraPosition - worldPos);
    vec3 H = normalize(L + V);

    float shininess = mix(128.0, 4.0, clamp(roughness, 0.0, 1.0));
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    vec3 ambient = uAmbientStrength * albedo;
    vec3 diffuse = NdotL * uLightColor * albedo;

    float spec = pow(NdotH, shininess);
    vec3 specularColor = mix(vec3(0.04), albedo, metallic);
    vec3 specular = spec * specularColor * uLightColor;

    oPixel = vec4(ambient + diffuse + specular, 1.0);
}