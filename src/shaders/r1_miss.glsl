#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(location = SWS_LOC_SECONDARY_RAY) rayPayloadInNVX RayPayload_s RayPayloadSecondary;

layout(set = SWS_IBL_SET, binding = SWS_IBL_BINDING) uniform sampler2D IBLTexture;

void main() {
    const vec3 dir = normalize(gl_WorldRayDirectionNVX);
    const vec2 uv = CartesianToLatLong(dir);

    const vec3 iblColor = texture(IBLTexture, uv).rgb;

    RayPayloadSecondary.colorAndDist = vec4(iblColor, -1.0f);
}
