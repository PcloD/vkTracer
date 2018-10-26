#ifndef SHARED_WITH_SHADERS_H
#define SHARED_WITH_SHADERS_H

#ifdef __cplusplus
// define some types to be compatible with GLSL
struct vec2 { float x, y; };
struct vec3 { float x, y, z; };
struct vec4 { float x, y, z, w; };
#endif // __cplusplus


// resource locations
#define SWS_SCENE_AS_SET        0
#define SWS_SCENE_AS_BINDING    0
#define SWS_OUT_IMAGE_SET       0
#define SWS_OUT_IMAGE_BINDING   1
#define SWS_IBL_SET             0
#define SWS_IBL_BINDING         2
#define SWS_MATERIALS_SET       0
#define SWS_MATERIALS_BINDING   3

#define SWS_FACEMATIDS_SET      1
#define SWS_FACES_SET           2
#define SWS_NORMALS_SET         3

// cross-shader locations
#define SWS_LOC_PRIMARY_RAY     0
#define SWS_LOC_HIT_ATTRIBS     1
#define SWS_LOC_SECONDARY_RAY   2


#define SWS_MAX_RECURSION       2


#define SWS_PI      3.1415926536f
#define SWS_EPSILON 0.000001f


struct RayPayload_s {
    vec4 colorAndDist;
};

struct Material_s {
    vec4 diffuse;
    vec4 emission;
};

#ifndef __cplusplus
// shaders helper functions
float Random(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898f, 78.233f))) * 43758.5453f);
}

vec3 BaryLerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics) {
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec2 CartesianToLatLong(vec3 dir) {
    const float u = (1.0f + atan(-dir.z, dir.x) / SWS_PI);
    const float v = acos(dir.y) / SWS_PI;
    return vec2(u * 0.5f, 1.0f - v);
}
#endif // __cplusplus

#endif // SHARED_WITH_SHADERS_H
