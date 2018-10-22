#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(set = 0, binding = 0) uniform accelerationStructureNVX SceneAS;
layout(set = 0, binding = 1, rgba8) uniform image2D OutputImage;

layout(location = 0) rayPayloadNVX RayPayload_s RayPayload;

void main() {
    const vec2 curPixel = vec2(gl_LaunchIDNVX.xy);
    const vec2 bottomRight = vec2(gl_LaunchSizeNVX.xy - 1);

    const vec2 uv = (curPixel / bottomRight) * 2.0f - 1.0f;

    const float aspect = float(gl_LaunchSizeNVX.y) / float(gl_LaunchSizeNVX.x);

    const vec3 origin = vec3(0.0f, 1.0f, 2.35f);
    const vec3 direction = vec3(uv.x, -uv.y * aspect, -1.0f);

    const uint rayFlags = gl_RayFlagsOpaqueNVX;
    const uint cullMask = 0xff;
    const float tmin = 0.001f;
    const float tmax = 10.0f;

    traceNVX(SceneAS,
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

    imageStore(OutputImage, ivec2(gl_LaunchIDNVX.xy), RayPayload.color);
}
