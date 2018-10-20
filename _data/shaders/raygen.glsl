#version 460
#extension GL_NVX_raytracing : require

layout(binding = 0) uniform accelerationStructureNVX topLevelAS;
layout(binding = 1, rgba8) uniform image2D image;

layout(location = 0) rayPayloadNVX vec3 hitValue;

void main() {
    const vec2 pixelCenter = vec2(gl_LaunchIDNVX.xy) + vec2(0.5f);
    const vec2 uv = pixelCenter / vec2(gl_LaunchSizeNVX.xy);

    vec3 origin = vec3(uv.x, 1.0f - uv.y, -1.0f);
    vec3 direction = vec3(0.0f, 0.0f, 1.0f);

    uint rayFlags = gl_RayFlagsOpaqueNVX;
    uint cullMask = 0xff;
    float tmin = 0.001f;
    float tmax = 100.0f;

    traceNVX(topLevelAS,
             rayFlags,
             cullMask,
             0 /*sbtRecordOffset*/,
             0 /*sbtRecordStride*/,
             0 /*missIndex*/,
             origin,
             tmin,
             direction,
             tmax,
             0 /*payload*/);

    imageStore(image, ivec2(gl_LaunchIDNVX.xy), vec4(hitValue, 0.0f));
}
