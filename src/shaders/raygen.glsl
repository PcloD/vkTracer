#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(binding = 0) uniform accelerationStructureNVX topLevelAS;
layout(binding = 1, rgba8) uniform image2D image;

layout(location = 0) rayPayloadNVX RayPayload_s RayPayload;

void main() {
    const vec2 pixelCenter = vec2(gl_LaunchIDNVX.xy) + vec2(0.5f);
    const vec2 uv = (pixelCenter / vec2(gl_LaunchSizeNVX.xy)) * 2.0f - 1.0f;

    const float aspect = float(gl_LaunchSizeNVX.y) / float(gl_LaunchSizeNVX.x);

    vec3 origin = vec3(0.0f, 1.0f, 2.35f);
    vec3 direction = vec3(uv.x, -uv.y * aspect, -1.0f);

    uint rayFlags = gl_RayFlagsOpaqueNVX;
    uint cullMask = 0xff;
    float tmin = 0.001f;
    float tmax = 10.0f;

    traceNVX(topLevelAS,
             rayFlags,
             cullMask,
             0 /*sbtRecordOffset*/,
             0 /*sbtRecordStride*/,
             0 /*missIndex*/,
             origin,
             tmin,
             direction,
             tmax,
             0 /*payload*/);

    imageStore(image, ivec2(gl_LaunchIDNVX.xy), RayPayload.color);
}
