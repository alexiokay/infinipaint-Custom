@echo off
setlocal
cd /d "%~dp0\..\..\build-arm64\build"
if errorlevel 1 exit /b 1
call generators\conanbuild.bat
if errorlevel 1 exit /b 1
cmake ..\.. -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DCONFIG_NEXT_TO_EXECUTABLE=ON -DGRAPHICS_BACKEND=Vulkan -DUSE_GRAPHITE=OFF -DINFINIPAINT_GEOMETRY_TESTS=ON "-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG /Zi" "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=/DEBUG:FULL /INCREMENTAL:NO /OPT:REF /OPT:ICF"
if errorlevel 1 exit /b 1
cmake --build . --config Release --parallel 3
if errorlevel 1 exit /b 1
exit /b 0
