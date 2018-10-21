#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(location = 0) rayPayloadInNVX RayPayload_s RayPayload;

void main() {
    vec2 uv = vec2(gl_LaunchIDNVX) / vec2(gl_LaunchSizeNVX);
    RayPayload.color = vec4(uv.x, 1.0f - uv.y, 0.0f, 1.0f);
}
