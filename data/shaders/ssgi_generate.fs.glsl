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
layout (location = 7) uniform float uStepSize;
layout (location = 8) uniform int uMaxSteps;
layout (location = 9) uniform float uThickness;

vec3 ReconstructViewPosition(vec2 uv, float depth) // This function reconstructs the view space position from screen UV and depth, which is a common step in screen-space effects to get the actual 3D position corresponding to a pixel
{
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = uInvProjection * clip;
    viewPos /= viewPos.w;
    return viewPos.xyz;
}

vec3 ReconstructWorldPosition(vec2 uv, float depth) // This function reconstructs the world position from screen UV and depth, which is essential for ray marching in world space
{
    vec3 viewPos = ReconstructViewPosition(uv, depth);
    vec4 worldPos = uInvView * vec4(viewPos, 1.0);
    return worldPos.xyz;
}

vec2 ProjectToScreenUV(vec3 worldPos) // This function projects a world position back to screen UV coordinates, which is essential for sampling the G-buffer during ray marching
{
    vec4 clip = uProjection * uView * vec4(worldPos, 1.0);
    clip /= clip.w;
    return clip.xy * 0.5 + 0.5;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / uScreenSize;
    float depth = texture(uGDepth, uv).r;

    if (depth >= 1.0) // If the depth is at the far plane, it means there's no geometry, so we can skip processing and output black/fallback color
    {
        oPixel = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 worldPos = ReconstructWorldPosition(uv, depth);
    vec3 normal = texture(uGNormal, uv).rgb * 2.0 - 1.0;
    normal = normalize(normal);
    vec3 albedo = texture(uGAlbedo, uv).rgb;
    vec2 material = texture(uGMaterial, uv).rg;
    
    vec3 rayDir = normalize(normal + vec3(0.0, 0.2, 0.0)); // first-direction choice: normal-biased hemisphere

    float stepSize = uStepSize;
    int maxSteps = uMaxSteps;
    float thickness = uThickness;

    bool hit = false;
    vec2 hitUV = vec2(0.0);
    vec3 hitPos = vec3(0.0);

    for (int i = 1; i <= maxSteps; ++i) // Start from 1 to avoid sampling the original pixel which would cause self-intersection
    {
        //vec3 samplePos = worldPos + rayDir * (stepSize * float(i)); // original choice: pure ray marching along the chosen direction
        vec3 samplePos = worldPos + normal * 0.05 + rayDir * (stepSize * float(i)); // second-direction choice: normal-biased hemisphere with an initial offset to avoid self-intersection
        vec2 sampleUV = ProjectToScreenUV(samplePos); // project the sample position back to screen space to fetch depth and other G-buffer data

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) // If the projected UV is outside the screen bounds, we can stop marching in this direction as it won't contribute to the final result
        {
            break;
        }

        float sceneDepth = texture(uGDepth, sampleUV).r;
        if (sceneDepth >= 1.0) // If the depth at the sample UV is at the far plane, it means there's no geometry there, so we can skip this sample and continue marching
        {
            continue;
        }

        vec3 scenePos = ReconstructWorldPosition(sampleUV, sceneDepth);
        float distanceToSurface = length(scenePos - samplePos);

        if (distanceToSurface < thickness) // If the distance from the sample position to the actual surface is less than the specified thickness, we consider it a valid hit for indirect lighting contribution
        {
            hit = true;
            hitUV = sampleUV;
            hitPos = scenePos;
            break;
        }
    }

    if (!hit) // If no valid hit is found after marching, output black/fallback color
    {
        oPixel = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 hitAlbedo = texture(uGAlbedo, hitUV).rgb;
    vec3 hitNormal = texture(uGNormal, hitUV).rgb * 2.0 - 1.0;
    hitNormal = normalize(hitNormal);
    vec3 toHit = normalize(hitPos - worldPos);

    float NdotL0 = max(dot(normal, toHit), 0.0);
    float NdotL1 = max(dot(hitNormal, -toHit), 0.0);

    float hitDistance = length(hitPos - worldPos);
    float attenuation = 1.0 / (1.0 + hitDistance * hitDistance);
    vec3 indirect = hitAlbedo * NdotL0 * NdotL1 * attenuation; // Simple diffuse-like contribution modulated by the angle between the original normal and the direction to the hit point, as well as the angle between the hit normal and the incoming direction, and attenuated by distance to avoid overly strong contributions from far hits
    indirect = min(indirect, vec3(0.5));
    oPixel = vec4(indirect * uSsgiIntensity, 1.0);
}