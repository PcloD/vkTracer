#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(set = SWS_SCENE_AS_SET,  binding = SWS_SCENE_AS_BINDING)         uniform accelerationStructureNVX SceneAS;
layout(set = SWS_OUT_IMAGE_SET, binding = SWS_OUT_IMAGE_BINDING, rgba8) uniform image2D OutputImage;

layout(location = SWS_LOC_PRIMARY_RAY) rayPayloadNVX RayPayload_s RayPayload;

void main() {
    const vec2 curPixel = vec2(gl_LaunchIDNVX.xy);
    const vec2 bottomRight = vec2(gl_LaunchSizeNVX.xy - 1);

    const vec2 uv = (curPixel / bottomRight) * 2.0f - 1.0f;

    const float aspect = float(gl_LaunchSizeNVX.y) / float(gl_LaunchSizeNVX.x);

    const vec3 origin = vec3(0.0f, 1.0f, 2.35f);
    const vec3 direction = normalize(vec3(uv.x, -uv.y * aspect, -1.0f));

    const uint rayFlags = gl_RayFlagsOpaqueNVX;
    const uint cullMask = 0xff;
    const float tmin = SWS_EPSILON;
    const float tmax = 100.0f;

    traceNVX(SceneAS,
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

    vec3 outColor = pow(RayPayload.colorAndDist.rgb, vec3(1.0f / 2.2f));

    imageStore(OutputImage, ivec2(gl_LaunchIDNVX.xy), vec4(outColor, 1.0f));
}
