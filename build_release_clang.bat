@echo off
cmake --preset=release-windows-clang
cmake --build --preset=release-windows-clang
start cmd /k ".\out\build\release\main.exe" 
