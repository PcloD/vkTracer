@echo off

setlocal
set GLSL_COMPILER=glslangValidator.exe
set SOURCE_FOLDER="./../src/shaders/"
set BINARIES_FOLDER="./shaders/"

%GLSL_COMPILER% -V -S rgen %SOURCE_FOLDER%raygen.glsl -o %BINARIES_FOLDER%raygen.bin
%GLSL_COMPILER% -V -S rchit %SOURCE_FOLDER%closest_hit.glsl -o %BINARIES_FOLDER%closest_hit.bin
%GLSL_COMPILER% -V -S rahit %SOURCE_FOLDER%any_hit.glsl -o %BINARIES_FOLDER%any_hit.bin
%GLSL_COMPILER% -V -S rmiss %SOURCE_FOLDER%ray_miss.glsl -o %BINARIES_FOLDER%ray_miss.bin
