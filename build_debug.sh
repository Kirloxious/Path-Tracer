#!/bin/sh

rm out/build/debug-linux/path-tracer
cmake --preset=debug-linux
cmake --build --preset=debug-linux
./out/build/debug-linux/path-tracer
