#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../shared_with_shaders.h"

layout(location = SWS_LOC_PRIMARY_RAY)   rayPayloadInNVX RayPayload_s RayPayload;
layout(location = SWS_LOC_HIT_ATTRIBS)   hitAttributeNVX vec3 HitAttribs;

layout(set = SWS_MATERIALS_SET, binding = SWS_MATERIALS_BINDING) readonly buffer MaterialsBuffer {
    Material_s Materials[];
};

layout(set = SWS_FACEMATIDS_SET, binding = 0) uniform usamplerBuffer FaceMatIDs[];
layout(set = SWS_FACES_SET,      binding = 0) uniform usamplerBuffer Faces[];
layout(set = SWS_NORMALS_SET,    binding = 0) uniform samplerBuffer Normals[];

void main() {
    const uint matID = texelFetch(FaceMatIDs[nonuniformEXT(gl_InstanceCustomIndexNVX)], gl_PrimitiveID).x;
    const Material_s material = Materials[nonuniformEXT(matID)];

    const vec3 barycentrics = vec3(1.0f - HitAttribs.x - HitAttribs.y, HitAttribs.x, HitAttribs.y);

    const uvec3 face = texelFetch(Faces[nonuniformEXT(gl_InstanceCustomIndexNVX)], gl_PrimitiveID).xyz;

    const vec3 n0 = texelFetch(Normals[nonuniformEXT(gl_InstanceCustomIndexNVX)], int(face.x)).xyz;
    const vec3 n1 = texelFetch(Normals[nonuniformEXT(gl_InstanceCustomIndexNVX)], int(face.y)).xyz;
    const vec3 n2 = texelFetch(Normals[nonuniformEXT(gl_InstanceCustomIndexNVX)], int(face.z)).xyz;

    const vec3 normal = normalize(mat3(gl_ObjectToWorldNVX) * BaryLerp(n0, n1, n2, barycentrics));

    RayPayload.colorAndDist = vec4(material.diffuse.rgb, gl_HitTNVX);
    RayPayload.normal = vec4(normal, 0.0f);
}
