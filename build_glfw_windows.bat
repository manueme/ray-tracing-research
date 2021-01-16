@echo off

cd .\external_sources
IF EXIST .\glfw (
    echo Pulling GLFW...
    cd .\glfw
    git checkout latest
    git pull
    cd ..\..
) ELSE (
    echo Cloning GLFW...
    git clone https://github.com/glfw/glfw.git
    cd ..
)

SET SOURCE_DIR=.\external_sources\glfw
echo Cleaning previous builds...
IF EXIST %SOURCE_DIR%\CMakeCache.txt DEL /F %SOURCE_DIR%\CMakeCache.txt
IF EXIST %SOURCE_DIR%\build (RMDIR %SOURCE_DIR%\build /S /Q)

SET GENERATOR=Visual Studio 16 2019

SET BINARIES_DIR=".\external_sources\glfw\build\x64"

echo Generating x64...
cmake CMakeLists.txt -G "%GENERATOR%" -A x64 -S %SOURCE_DIR% -B %BINARIES_DIR%
echo Building x64 Debug...
cmake --build %BINARIES_DIR% --config debug
echo Building x64 Release...
cmake --build %BINARIES_DIR% --config release
