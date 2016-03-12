$ErrorActionPreference="Stop"

mkdir build
cd build
cmake -G "Visual Studio 14" -DCMAKE_BUILD_TYPE=Release ..\src
cmake --build . --config Release

