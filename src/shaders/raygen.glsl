#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(set = SWS_SCENE_AS_SET,  binding = SWS_SCENE_AS_BINDING)         uniform accelerationStructureNVX Scene;
layout(set = SWS_OUT_IMAGE_SET, binding = SWS_OUT_IMAGE_BINDING, rgba8) uniform image2D OutputImage;

layout(set = SWS_CAMDATA_SET, binding = SWS_CAMDATA_BINDING, std140) uniform CamData {
    CamData_s Camera;
};

layout(location = SWS_LOC_PRIMARY_RAY)   rayPayloadNVX RayPayload_s RayPayload;
layout(location = SWS_LOC_SECONDARY_RAY) rayPayloadNVX RayPayload_s RayPayloadSecondary;


vec3 CalcRayDir(vec2 screenUV, float aspect) {
    vec3 u = Camera.side.xyz;
    vec3 v = Camera.up.xyz;

    const float planeWidth = tan(Camera.nearFarFov.z * 0.5f);

    u *= (planeWidth * aspect);
    v *= planeWidth;

    //const vec3 rayDir = normalize(Camera.dir.xyz + (u * screenUV.x) - (v * (screenUV.y - 1.0f)));
    const vec3 rayDir = normalize(Camera.dir.xyz + (u * screenUV.x) - (v * screenUV.y));
    return rayDir;
}

const vec3 gSunPos = vec3(436.181488f, 583.134888f, 57.8915443f);

void main() {
    const vec2 curPixel = vec2(gl_LaunchIDNVX.xy);
    const vec2 bottomRight = vec2(gl_LaunchSizeNVX.xy - 1);

    const vec2 uv = (curPixel / bottomRight) * 2.0f - 1.0f;

    const float aspect = float(gl_LaunchSizeNVX.x) / float(gl_LaunchSizeNVX.y);

    const vec3 origin = Camera.pos.xyz;
    const vec3 direction = CalcRayDir(uv, aspect);

    const uint rayFlags = gl_RayFlagsOpaqueNVX;
    const uint cullMask = 0xff;
    const float tmin = Camera.nearFarFov.x;
    const float tmax = Camera.nearFarFov.y;

    traceNVX(Scene,
             rayFlags,
             cullMask,
             0 /*sbtRecordOffset*/,
             0 /*sbtRecordStride*/,
             0 /*missIndex*/,
             origin,
             tmin,
             direction,
             tmax,
             SWS_LOC_PRIMARY_RAY);

    const vec3 hitColor = RayPayload.colorAndDist.rgb;
    const float hitT = RayPayload.colorAndDist.w;
    const vec3 hitNormal = RayPayload.normal.xyz;

    float lambert = 1.0f;
    if (hitT > SWS_EPSILON) {
        const vec3 hitPos = origin + direction * hitT;
        vec3 toLight = gSunPos - hitPos;
        const float toLightDist = length(toLight);
        toLight /= toLightDist;

        // check for shadow
        traceNVX(Scene,
                 rayFlags,
                 cullMask,
                 1 /*sbtRecordOffset*/,
                 0 /*sbtRecordStride*/,
                 1 /*missIndex*/,
                 hitPos + (hitNormal * 0.1f),
                 SWS_EPSILON,
                 toLight,
                 toLightDist,
                 SWS_LOC_SECONDARY_RAY);

        if (RayPayloadSecondary.colorAndDist.w < toLightDist) {
            // in shadow
            lambert = 0.05f;
        } else {
            lambert = max(0.05f, dot(hitNormal, toLight));
        }
    }

    const vec3 outColor = hitColor * lambert;

    imageStore(OutputImage, ivec2(gl_LaunchIDNVX.xy), vec4(LinearToSrgb(outColor), 1.0f));
}
