#version 460 core

layout (location = 0) out vec4 oPixel;

layout (binding = 0) uniform sampler2D uGAlbedo;
layout (binding = 1) uniform sampler2D uGNormal;
layout (binding = 2) uniform sampler2D uGMaterial;
layout (binding = 3) uniform sampler2D uGDepth;
layout (binding = 4) uniform sampler2D uSsgiRaw;
layout (binding = 5) uniform sampler2D uSsgiFiltered;
layout (binding = 6) uniform sampler2D uSsgiTemporal;
layout (binding = 7) uniform sampler2D uSsgiFinal;
layout (binding = 8) uniform sampler2DShadow uShadowMap;

layout (location = 0) uniform vec2 uScreenSize;
layout (location = 1) uniform mat4 uInvProjection;
layout (location = 2) uniform mat4 uInvView;

layout (location = 3) uniform vec3 uLightDirection;
layout (location = 4) uniform vec3 uLightColor;
layout (location = 5) uniform float uAmbientStrength;
layout (location = 6) uniform vec3 uCameraPosition;
layout (location = 7) uniform int uDebugView;
layout (location = 8) uniform int uDenoiserMode;
layout (location = 9) uniform int uEnableSsgi;
layout (location = 10) uniform mat4 uLightViewProjection;
layout (location = 11) uniform int uEnableShadows;
layout (location = 12) uniform float uShadowBiasMin;
layout (location = 13) uniform float uShadowBiasMax;

vec3 ReconstructWorldPosition(vec2 uv, float depth)
{
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = uInvProjection * clip;
    viewPos /= viewPos.w;
    vec4 worldPos = uInvView * viewPos;
    return worldPos.xyz;
}

float ComputeShadow(vec3 worldPos, vec3 N, vec3 L)
{
    vec4 lightClip = uLightViewProjection * vec4(worldPos, 1.0);
    vec3 lightNdc = lightClip.xyz / lightClip.w;

    vec2 shadowUV = lightNdc.xy * 0.5 + 0.5;
    float currentDepth = lightNdc.z * 0.5 + 0.5;

    if (lightClip.w <= 0.0)
    {
        return 1.0;
    }

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0)
    {
        return 1.0;
    }

    if (currentDepth <= 0.0 || currentDepth >= 1.0)
    {
        return 1.0;
    }

    float ndotl = max(dot(N, L), 0.0);
    float bias = max(uShadowBiasMax * (1.0 - ndotl), uShadowBiasMin);

    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));

    float visibility = 0.0;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(x, y) * texelSize;
            visibility += texture(uShadowMap, vec3(shadowUV + offset, currentDepth - bias));
        }
    }

    return visibility / 25.0;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / uScreenSize;

    vec3 albedo = texture(uGAlbedo, uv).rgb;
    vec3 normal = texture(uGNormal, uv).rgb * 2.0 - 1.0;
    vec2 material = texture(uGMaterial, uv).rg;
    float depth = texture(uGDepth, uv).r;
    vec3 rawSsgi = texture(uSsgiRaw, uv).rgb;
    vec3 filteredSsgi = texture(uSsgiFiltered, uv).rgb;
    vec3 temporalSsgi = texture(uSsgiTemporal, uv).rgb;

    if (depth >= 1.0)
    {
        oPixel = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

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

    if (uDebugView == 5)
    {
        vec3 p = worldPos * 0.05;
        oPixel = vec4(fract(abs(p)), 1.0);
        return;
    }
    if (uDebugView == 6)
    {
        oPixel = vec4(rawSsgi, 1.0);
        return;
    }

    if (uDebugView == 7)
    {
        oPixel = vec4(filteredSsgi, 1.0);
        return;
    }
    if (uDebugView == 8)
    {
        oPixel = vec4(temporalSsgi, 1.0);
        return;
    }

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
    
    float shadow = 1.0;
    if (uEnableShadows != 0 && NdotL > 0.0)
    {
        shadow = ComputeShadow(worldPos, N, L);
    }

    if (uDebugView == 9)
    {
        oPixel = vec4(vec3(shadow), 1.0);
        return;
    }

    vec3 direct = ambient + shadow * (diffuse + specular);

    vec3 indirect = vec3(0.0);
    if (uEnableSsgi != 0)
    {
        indirect = texture(uSsgiFinal, uv).rgb;
    }

    vec3 finalColor = direct + indirect * 0.5;
    oPixel = vec4(finalColor, 1.0);
}