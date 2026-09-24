#!/bin/sh
# Usage: ./build.sh [debug|release] [--no-run]
set -e

cd "$(dirname "$0")"

config=debug
run=1
for arg in "$@"; do
    case "$arg" in
        debug | release) config="$arg" ;;
        --no-run) run=0 ;;
        -h | --help)
            echo "Usage: $0 [debug|release] [--no-run]"
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            echo "Usage: $0 [debug|release] [--no-run]" >&2
            exit 1
            ;;
    esac
done

case "$(uname -s)" in
    Darwin) preset="$config-macos" ;;
    *) preset="$config-linux" ;;
esac

cmake --preset="$preset"
cmake --build --preset="$preset"

if [ "$run" -eq 1 ]; then
    exec "./out/build/$preset/path-tracer"
fi
