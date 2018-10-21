#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(location = 0) rayPayloadInNVX RayPayload_s RayPayload;
layout(location = 1) hitAttributeNVX vec3 attribs;

void main() {
#if 0
    const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    RayPayload.color = vec4(barycentrics, 1.0f);
#else
    float t = float(gl_InstanceCustomIndexNVX) / 8.0f;
    RayPayload.color = vec4(t, t, t, 1.0f);
#endif
}
