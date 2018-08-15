﻿#version 450 core

#define MAX_GEO_PIXELS 32768
#define MAX_AIR_PIXELS 32768

#define MAX_GEO_PER_AIR 3



layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

// Types -----------------------------------------------------------------------

struct GeoPixel {
    vec4 worldPos;
    vec4 normal;
};

struct AirPixel {
    vec4 worldPos;
    vec4 velocity;
};

struct AirGeoMapElement {
    int geoCount;
    int geoIndices[MAX_GEO_PER_AIR];
};

// Constants -------------------------------------------------------------------

const int k_invocCount = 1024;

const bool k_distinguishActivePixels = true; // Makes certain "active" pixels brigher for visual clarity, but lowers performance

const float k_greenVal = k_distinguishActivePixels ? 1.0f / 3.0f : 1.0f;

// Uniforms --------------------------------------------------------------------

layout (binding = 0, rgba8) uniform image2D u_fboImg;
layout (binding = 3, r32i) uniform iimage2D u_flagImg;
layout (binding = 4, rgba8) uniform image2D u_sideImg;

layout (binding = 0, std430) restrict buffer SSBO {
    int u_swap;
    int u_geoCount;
    int u_airCount[2];
    ivec2 u_screenSize;
    vec2 u_screenAspectFactor;
    float u_sliceSize;
    float u_windSpeed;
    float u_dt;
    int _0; // padding
    ivec4 u_momentum;
    ivec4 u_force;
    ivec4 u_dragForce;
    ivec4 u_dragMomentum;
};

// Done this way because having a lot of large static sized arrays makes shader compilation super slow for some reason
layout (binding = 1, std430) buffer GeoPixels { // TODO: should be restrict?
    GeoPixel u_geoPixels[];
};
layout (binding = 2, std430) buffer AirPixels { // TODO: should be restrict?
    AirPixel u_airPixels[];
};
layout (binding = 3, std430) buffer AirGeoMap { // TODO: should be restrict?
    AirGeoMapElement u_airGeoMap[];
};

// Functions -------------------------------------------------------------------

vec2 worldToScreen(vec3 world) {
    vec2 screenPos = world.xy;
    screenPos *= u_screenAspectFactor; // compensate for aspect ratio
    screenPos = screenPos * 0.5f + 0.5f; // center
    screenPos *= vec2(u_screenSize); // scale to texture space
    return screenPos;
}

void main() {
    int counterSwap = 1 - u_swap;

    int invocI = int(gl_GlobalInvocationID.x);
    int invocWorkload = (u_airCount[counterSwap] + k_invocCount - 1) / k_invocCount;
    for (int ii = 0; ii < invocWorkload; ++ii) {

        int prevAirI = invocI + (k_invocCount * ii);
        if (prevAirI >= u_airCount[counterSwap]) {
            return;
        }

        vec3 worldPos = u_airPixels[prevAirI + counterSwap * MAX_AIR_PIXELS].worldPos.xyz;
        vec3 velocity = u_airPixels[prevAirI + counterSwap * MAX_AIR_PIXELS].velocity.xyz;
        
        ivec2 texCoord = ivec2(worldToScreen(worldPos));
        ivec2 sideTexCoord = ivec2(worldToScreen(vec3(-worldPos.z, worldPos.y, 0)));

        // Check if in texture
        if (texCoord.x < 0 || texCoord.y < 0 || texCoord.x >= u_screenSize.x || texCoord.y >= u_screenSize.y) {
            continue;
        }
        // Check if in geometry
        vec4 color = imageLoad(u_fboImg, texCoord);
        if (color.r != 0.0f) {
            continue;
        }
        
        // Move to current air pixel buffer
        int airI = atomicAdd(u_airCount[u_swap], 1);
        if (airI >= MAX_AIR_PIXELS) {
            return;
        }
        u_airPixels[airI + u_swap * MAX_AIR_PIXELS].worldPos = vec4(worldPos, 0.0f);
        u_airPixels[airI + u_swap * MAX_AIR_PIXELS].velocity = vec4(velocity, 0.0f);
        u_airGeoMap[airI].geoCount = 0; // This air pixel is not yet associated with any geometry

        // Store air index
        imageAtomicExchange(u_flagImg, texCoord, airI + 1);
        
        // Draw to front view
        color.g = k_greenVal;
        imageStore(u_fboImg, texCoord, color);

        // Draw to side view
        color = imageLoad(u_sideImg, sideTexCoord);
        color.g = k_greenVal;
        imageStore(u_sideImg, sideTexCoord, color);
    }        
}