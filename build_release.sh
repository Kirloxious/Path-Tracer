#!/bin/sh

rm out/build/release-linux/path-tracer
cmake --preset=release-linux
cmake --build --preset=release-linux
./out/build/release-linux/path-tracer
