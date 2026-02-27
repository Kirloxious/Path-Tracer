#!/bin/sh

rm out/build/debug-linux/main
cmake --preset=debug-linux
cmake --build --preset=debug-linux
chmod +x /out/build/debug-linux/main
./out/build/debug-linux/main