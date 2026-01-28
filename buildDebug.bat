@echo off
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64
cmake --build out/build --config Debug
pause