@echo off

setlocal
set GLSL_COMPILER=glslangValidator.exe
set SOURCE_FOLDER="./../src/shaders/"
set BINARIES_FOLDER="./shaders/"

:: raygen shaders
%GLSL_COMPILER% -V -S rgen %SOURCE_FOLDER%raygen.glsl -o %BINARIES_FOLDER%raygen.bin

:: closest hit shaders
%GLSL_COMPILER% -V -S rchit %SOURCE_FOLDER%r0_chit.glsl -o %BINARIES_FOLDER%r0_chit.bin
%GLSL_COMPILER% -V -S rchit %SOURCE_FOLDER%r1_chit.glsl -o %BINARIES_FOLDER%r1_chit.bin

:: any hit shaders
::%GLSL_COMPILER% -V -S rahit %SOURCE_FOLDER%any_hit.glsl -o %BINARIES_FOLDER%any_hit.bin

:: miss shaders
%GLSL_COMPILER% -V -S rmiss %SOURCE_FOLDER%r0_miss.glsl -o %BINARIES_FOLDER%r0_miss.bin
%GLSL_COMPILER% -V -S rmiss %SOURCE_FOLDER%r1_miss.glsl -o %BINARIES_FOLDER%r1_miss.bin
