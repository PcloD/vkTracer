#version 460
#extension GL_NVX_raytracing : require

layout(location = 0) rayPayloadInNVX vec3 hitValue;

void main() {
    hitValue = vec3(gl_WorldRayOriginNVX.x, gl_WorldRayOriginNVX.y, 0.0f);
}
