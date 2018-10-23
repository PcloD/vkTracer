#ifndef SHARED_WITH_SHADERS_H
#define SHARED_WITH_SHADERS_H

#ifdef __cplusplus
// define some types to be compatible with GLSL
struct vec2 { float x, y; };
struct vec3 { float x, y, z; };
struct vec4 { float x, y, z, w; };
#endif // __cplusplus

#define MY_EPSILON  0.000001f


struct RayPayload_s {
    vec4 color;
};

struct Material_s {
    vec4 diffuse;
    vec4 emission;
};

#ifndef __cplusplus
// shaders helper functions
vec3 BaryLerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics) {
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}
#endif // __cplusplus

#endif // SHARED_WITH_SHADERS_H
