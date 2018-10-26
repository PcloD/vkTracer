#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../shared_with_shaders.h"

layout(location = SWS_LOC_SECONDARY_RAY) rayPayloadInNVX RayPayload_s RayPayloadSecondary;

layout(set = SWS_MATERIALS_SET, binding = SWS_MATERIALS_BINDING) buffer MaterialsBuffer { Material_s Materials[]; };

layout(set = SWS_FACEMATIDS_SET, binding = 0) uniform usamplerBuffer FaceMatIDs[];

void main() {
    const uint matID = texelFetch(FaceMatIDs[nonuniformEXT(gl_InstanceCustomIndexNVX)], gl_PrimitiveID).x;
    const Material_s material = Materials[nonuniformEXT(matID)];

    RayPayloadSecondary.colorAndDist = vec4(material.emission.rgb, gl_HitTNVX);
}
