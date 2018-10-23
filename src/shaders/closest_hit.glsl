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
layout(set = 2, binding = 0) uniform usamplerBuffer Faces[];
layout(set = 3, binding = 0) uniform samplerBuffer Normals[];

void main() {
    vec3 color = vec3(0.0f);

    const uint matID = texelFetch(FaceMatIDs[nonuniformEXT(gl_InstanceCustomIndexNVX)], gl_PrimitiveID).x;
    const Material_s material = Materials[nonuniformEXT(matID)];

    if (length(material.emission.rgb) > MY_EPSILON) {
        color = material.emission.rgb;
    } else {
        const vec3 barycentrics = vec3(1.0f - HitAttribs.x - HitAttribs.y, HitAttribs.x, HitAttribs.y);

        const uvec3 face = texelFetch(Faces[nonuniformEXT(gl_InstanceCustomIndexNVX)], gl_PrimitiveID).xyz;

        const vec3 n0 = texelFetch(Normals[nonuniformEXT(gl_InstanceCustomIndexNVX)], int(face.x)).xyz;
        const vec3 n1 = texelFetch(Normals[nonuniformEXT(gl_InstanceCustomIndexNVX)], int(face.y)).xyz;
        const vec3 n2 = texelFetch(Normals[nonuniformEXT(gl_InstanceCustomIndexNVX)], int(face.z)).xyz;

        const vec3 normal = normalize(mat3(gl_ObjectToWorldNVX) * BaryLerp(n0, n1, n2, barycentrics));

        color = normal * 0.5f + 0.5f;
    }

    RayPayload.color = vec4(color, 1.0f);
}
