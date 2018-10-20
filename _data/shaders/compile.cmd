@echo off

glslangValidator.exe -V -S rgen raygen.glsl -o raygen.bin
glslangValidator.exe -V -S rchit closest_hit.glsl -o closest_hit.bin
glslangValidator.exe -V -S rahit any_hit.glsl -o any_hit.bin
glslangValidator.exe -V -S rmiss ray_miss.glsl -o ray_miss.bin
