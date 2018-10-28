#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(location = SWS_LOC_SECONDARY_RAY) rayPayloadInNVX RayPayload_s RayPayloadSecondary;

void main() {
    RayPayloadSecondary.colorAndDist = vec4(gl_HitTNVX);

#ifdef SHADER_ANY_HIT_VARIATION
    terminateRayNVX();
#endif
}
