#version 460 core

layout (location = 0) out vec4 oPixel;

layout (binding = 0) uniform sampler2D uGAlbedo;
layout (binding = 1) uniform sampler2D uGNormal;
layout (binding = 2) uniform sampler2D uGMaterial;
layout (binding = 3) uniform sampler2D uGDepth;

layout (location = 0) uniform vec2 uScreenSize;
layout (location = 1) uniform mat4 uProjection;
layout (location = 2) uniform mat4 uInvProjection;
layout (location = 3) uniform mat4 uView;
layout (location = 4) uniform mat4 uInvView;
layout (location = 5) uniform vec3 uCameraPosition;
layout (location = 6) uniform float uSsgiIntensity;
layout (location = 7) uniform vec3 uLightDirection;
layout (location = 8) uniform vec3 uLightColor;
layout (location = 9) uniform float uAmbientStrength;

vec3 ReconstructViewPosition(vec2 uv, float depth)
{
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = uInvProjection * clip;
    viewPos /= viewPos.w;
    return viewPos.xyz;
}

vec3 ReconstructWorldPosition(vec2 uv, float depth)
{
    vec3 viewPos = ReconstructViewPosition(uv, depth);
    vec4 worldPos = uInvView * vec4(viewPos, 1.0);
    return worldPos.xyz;
}

vec2 ProjectToScreenUV(vec3 worldPos)
{
    vec4 clip = uProjection * uView * vec4(worldPos, 1.0);
    clip /= clip.w;
    return clip.xy * 0.5 + 0.5;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / uScreenSize;
    float depth = texture(uGDepth, uv).r;

    if (depth >= 1.0)
    {
        oPixel = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 worldPos = ReconstructWorldPosition(uv, depth);
    vec3 normal = texture(uGNormal, uv).rgb * 2.0 - 1.0;
    normal = normalize(normal);
    vec3 albedo = texture(uGAlbedo, uv).rgb;
    vec2 material = texture(uGMaterial, uv).rg;

    float roughness = material.r;
    float metallic = material.g;

    vec3 L = normalize(-uLightDirection);
    vec3 V = normalize(uCameraPosition - worldPos);
    vec3 H = normalize(L + V);

    float shininess = mix(128.0, 4.0, clamp(roughness, 0.0, 1.0));
    float NdotL = max(dot(normal, L), 0.0);
    float NdotH = max(dot(normal, H), 0.0);

    vec3 ambient = uAmbientStrength * albedo;
    vec3 diffuse = NdotL * uLightColor * albedo;

    float spec = pow(NdotH, shininess);
    vec3 specularColor = mix(vec3(0.04), albedo, metallic);
    vec3 specular = spec * specularColor * uLightColor;

    vec3 direct = ambient + diffuse + specular;
    
    vec3 rayDir = normalize(normal + vec3(0.0, 0.2, 0.0)); // first-direction choice: normal-biased hemisphere

    float stepSize = 0.05;
    int maxSteps = 96;
    float thickness = 0.1;

    bool hit = false;
    vec2 hitUV = vec2(0.0);
    vec3 hitPos = vec3(0.0);

    for (int i = 1; i <= maxSteps; ++i)
    {
        //vec3 samplePos = worldPos + rayDir * (stepSize * float(i)); // original choice: pure ray marching along the chosen direction
        vec3 samplePos = worldPos + normal * 0.05 + rayDir * (stepSize * float(i)); // second-direction choice: normal-biased hemisphere with an initial offset to avoid self-intersection
        vec2 sampleUV = ProjectToScreenUV(samplePos); // project the sample position back to screen space to fetch depth and other G-buffer data

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
        {
            break;
        }

        float sceneDepth = texture(uGDepth, sampleUV).r;
        if (sceneDepth >= 1.0)
        {
            continue;
        }

        vec3 scenePos = ReconstructWorldPosition(sampleUV, sceneDepth);
        float distanceToSurface = length(scenePos - samplePos);

        if (distanceToSurface < thickness)
        {
            hit = true;
            hitUV = sampleUV;
            hitPos = scenePos;
            break;
        }
    }

    if (!hit)
    {
        oPixel = vec4(direct, 1.0); // if no hit was found, output the direct lighting alone
        return;
    }

    vec3 hitAlbedo = texture(uGAlbedo, hitUV).rgb;
    vec3 hitNormal = texture(uGNormal, hitUV).rgb * 2.0 - 1.0;
    hitNormal = normalize(hitNormal);

    vec3 toHit = normalize(hitPos - worldPos);

    float NdotL0 = max(dot(normal, toHit), 0.0);
    float NdotL1 = max(dot(hitNormal, -toHit), 0.0);

    vec3 indirect = hitAlbedo * NdotL0 * NdotL1; // simple visibility-weighted indirect term

    //oPixel = vec4(indirect, 1.0); // debug: visualize the indirect contribution alone
    vec3 finalColor = direct + indirect * uSsgiIntensity; // combine direct and indirect contributions, allowing control over the SSGI intensity with a uniform
    
    oPixel = vec4(finalColor, 1.0); // if a hit was found, output the combined direct and indirect lighting
}