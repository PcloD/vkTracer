#ifndef SHARED_WITH_SHADERS_H
#define SHARED_WITH_SHADERS_H

#ifdef __cplusplus
// include math header for vec & mat types (same namings as in GLSL)
#include "mymath.h"
#endif // __cplusplus


// resource locations
#define SWS_SCENE_AS_SET        0
#define SWS_SCENE_AS_BINDING    0
#define SWS_OUT_IMAGE_SET       0
#define SWS_OUT_IMAGE_BINDING   1
#define SWS_CAMDATA_SET         0
#define SWS_CAMDATA_BINDING     2
#define SWS_IBL_SET             0
#define SWS_IBL_BINDING         3
#define SWS_MATERIALS_SET       0
#define SWS_MATERIALS_BINDING   4

#define SWS_FACEMATIDS_SET      1
#define SWS_FACES_SET           2
#define SWS_NORMALS_SET         3

// cross-shader locations
#define SWS_LOC_PRIMARY_RAY     0
#define SWS_LOC_HIT_ATTRIBS     1
#define SWS_LOC_SECONDARY_RAY   2


#define SWS_MAX_RECURSION       2


#define SWS_PI      3.1415926536f
#define SWS_EPSILON 1e-5f

struct CamData_s {
    vec4 pos;
    vec4 dir;
    vec4 up;
    vec4 side;
    vec4 nearFarFov;
};

struct RayPayload_s {
    vec4 colorAndDist;
    vec4 normal;
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

vec3 LinearToSrgb(vec3 c) {
#if 0
    // Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    vec3 sq1 = sqrt(c);
    vec3 sq2 = sqrt(sq1);
    vec3 sq3 = sqrt(sq2);
    vec3 srgb = sq1 * 0.662002687f + sq2 * 0.684122060f - sq3 * 0.323583601f - c * 0.0225411470f;
#else
    vec3 srgb = pow(c, vec3(1.0f / 2.2f));
#endif
    return srgb;
}

vec2 BaryLerp(vec2 a, vec2 b, vec2 c, vec3 barycentrics) {
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 BaryLerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics) {
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec2 CartesianToLatLong(vec3 dir) {
    const float u = (1.0f + atan(-dir.z, dir.x) / SWS_PI);
    const float v = acos(dir.y) / SWS_PI;
    return vec2(u * 0.5f, v);
}
#endif // __cplusplus

#endif // SHARED_WITH_SHADERS_H
