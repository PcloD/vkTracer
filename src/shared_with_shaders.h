#ifndef SHARED_WITH_SHADERS_H
#define SHARED_WITH_SHADERS_H

#ifdef __cplusplus
// define some types to be compatible with GLSL
using vec2 = float[2];
using vec3 = float[3];
using vec4 = float[4];
#endif // __cplusplus


struct RayPayload_s {
    vec4 color;
};

struct Material_s {
    vec4 diffuse;
    vec4 emission;
};

#endif // SHARED_WITH_SHADERS_H
