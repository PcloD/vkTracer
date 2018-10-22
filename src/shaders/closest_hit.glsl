#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../shared_with_shaders.h"

layout(location = 0) rayPayloadInNVX RayPayload_s RayPayload;
layout(location = 1) hitAttributeNVX vec3 HitAttribs;

layout(set = 0, binding = 2) buffer MaterialsBuffer {
    Material_s Materials[];
};

layout(set = 1, binding = 0) uniform usamplerBuffer FaceMatIDs[];

void main() {
    const uint matID = texelFetch(FaceMatIDs[nonuniformEXT(gl_InstanceCustomIndexNVX)], gl_PrimitiveID).r;
    const Material_s material = Materials[nonuniformEXT(matID)];

    RayPayload.color = material.diffuse;
}
