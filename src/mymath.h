#pragma once

#pragma warning(push)
#pragma warning(disable : 4201) // C4201: nonstandard extension used: nameless struct/union
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#pragma warning(pop)

using vec2 = glm::highp_vec2;
using vec3 = glm::highp_vec3;
using vec4 = glm::highp_vec4;
using mat4 = glm::highp_mat4;
using quat = glm::highp_quat;

struct Recti { int left, top, right, bottom; };

static const float MM_Pi = 3.1415926536f;

inline float Rad2Deg(const float rad) {
    return rad * (180.0f / MM_Pi);
}

inline float Deg2Rad(const float deg) {
    return deg * (MM_Pi / 180.0f);
}

template <typename T>
inline T max(const T& a, const T& b) {
    return a > b ? a : b;
}

template <typename T>
inline T min(const T& a, const T& b) {
    return a < b ? a : b;
}

template <typename T>
inline T lerp(const T& a, const T& b, const float t) {
    return a + (b - a) * t;
}

template <typename T>
inline T clamp(const T& v, const T& minV, const T& maxV) {
    return (v < minV) ? minV : ((v > maxV) ? maxV : v);
}

// these funcs should be compatible with same in GLSL
template <typename T>
inline float length(const T& v) { return glm::length(v); }
inline float dot(const vec2& a, const vec2& b) { return glm::dot(a, b); }
inline float dot(const vec3& a, const vec3& b) { return glm::dot(a, b); }
inline float dot(const vec4& a, const vec4& b) { return glm::dot(a, b); }
inline vec3 cross(const vec3& a, const vec3& b) { return glm::cross(a, b); }
inline vec3 normalize(const vec3& v) { return glm::normalize(v); }

inline quat normalize(const quat& q) { return glm::normalize(q); }
inline quat QAngleAxis(const float angleRad, const vec3& axis) { return glm::angleAxis(angleRad, axis); }
inline vec3 QRotate(const quat& q, const vec3& v) { return glm::rotate(q, v); }

inline mat4 MatRotate(const float angle, const float x, const float y, const float z) { return glm::rotate(angle, vec3(x, y, z)); }


// aliases to glm's functions

//constexpr auto MatProjection = glm::perspective<float>;
inline mat4 MatProjection(const float fovY, const float aspect, const float nearZ, const float farZ) {
    mat4 m = glm::perspectiveRH_ZO(fovY, aspect, nearZ, farZ);
    //#NOTE_SK: Ugly fucking hack to support Vulkans NDC (Y goes down)
    m[1][1] *= -1.0f;
    return m;
}

constexpr auto MatOrtho = glm::orthoRH<float>;
constexpr auto MatLookAt = glm::lookAtRH<float, glm::highp>;

constexpr auto QToMat = glm::toMat4<float, glm::highp>;
