#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../shared_with_shaders.h"

layout(location = SWS_LOC_PRIMARY_RAY)   rayPayloadInNVX RayPayload_s RayPayload;
layout(location = SWS_LOC_HIT_ATTRIBS)   hitAttributeNVX vec3 HitAttribs;
layout(location = SWS_LOC_SECONDARY_RAY) rayPayloadNVX RayPayload_s RayPayloadSecondary;

layout(set = SWS_SCENE_AS_SET,  binding = SWS_SCENE_AS_BINDING)  uniform accelerationStructureNVX SceneAS;
layout(set = SWS_MATERIALS_SET, binding = SWS_MATERIALS_BINDING) buffer MaterialsBuffer { Material_s Materials[]; };

layout(set = SWS_FACEMATIDS_SET, binding = 0) uniform usamplerBuffer FaceMatIDs[];
layout(set = SWS_FACES_SET,      binding = 0) uniform usamplerBuffer Faces[];
layout(set = SWS_NORMALS_SET,    binding = 0) uniform samplerBuffer Normals[];

// TODO: replace with extern buffer
vec3 RandomDir(vec2 t) {
    vec3 result;

    float r = Random(t);

    result.x = Random(t + r);
    result.y = Random(t + result.x);
    result.z = Random(t + t + result.y + r);

    result.xyz = result.xyz * 2.0f - 1.0f;

    return result;
}

void main() {
    vec3 color = vec3(0.0f);

    const uint matID = texelFetch(FaceMatIDs[nonuniformEXT(gl_InstanceCustomIndexNVX)], gl_PrimitiveID).x;
    const Material_s material = Materials[nonuniformEXT(matID)];

    if (length(material.emission.rgb) > SWS_EPSILON) {
        color = material.emission.rgb;
    } else {
        const vec3 barycentrics = vec3(1.0f - HitAttribs.x - HitAttribs.y, HitAttribs.x, HitAttribs.y);

        const uvec3 face = texelFetch(Faces[nonuniformEXT(gl_InstanceCustomIndexNVX)], gl_PrimitiveID).xyz;

        const vec3 n0 = texelFetch(Normals[nonuniformEXT(gl_InstanceCustomIndexNVX)], int(face.x)).xyz;
        const vec3 n1 = texelFetch(Normals[nonuniformEXT(gl_InstanceCustomIndexNVX)], int(face.y)).xyz;
        const vec3 n2 = texelFetch(Normals[nonuniformEXT(gl_InstanceCustomIndexNVX)], int(face.z)).xyz;

        const vec3 normal = normalize(mat3(gl_ObjectToWorldNVX) * BaryLerp(n0, n1, n2, barycentrics));

        // reconstruct hit pos
        const vec3 origin = gl_WorldRayOriginNVX + gl_WorldRayDirectionNVX * gl_HitTNVX;

        const uint rayFlags = gl_RayFlagsOpaqueNVX;
        const uint cullMask = 0xff;
        const float tmin = SWS_EPSILON;
        const float tmax = 100.0f;

#define NUM_SPP 256

        vec3 irradiance = vec3(0.0f);
        for (int i = 0; i < NUM_SPP; ++i) {
            const float t0 = float(i) / float(NUM_SPP - 1);
            const float t1 = 1.0f - t0;
            const vec3 direction = RandomDir(vec2(gl_LaunchIDNVX) / vec2(gl_LaunchSizeNVX) + vec2(t0, t1));

            traceNVX(SceneAS,
                     rayFlags,
                     cullMask,
                     1 /* sbtRecordOffset */,
                     0 /* sbtRecordStride */,
                     1 /* missIndex */,
                     origin,
                     tmin,
                     direction,
                     tmax,
                     2 /*payload*/);

            irradiance += RayPayloadSecondary.colorAndDist.rgb;
        }

        irradiance /= float(NUM_SPP);

        //color = normal * 0.5f + 0.5f;
        color = material.diffuse.rgb * irradiance;
    }

    RayPayload.colorAndDist = vec4(color, gl_HitTNVX);
}
