#!/bin/sh
#
# Use this script to run your program LOCALLY.


set -e # Exit early if any commands fail

(
  cd "$(dirname "$0")" # Ensure compile steps are run within the repository directory
  if [ -n "$VCPKG_ROOT" ]; then
    cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  else
    cmake -B build -S .
  fi
  cmake --build ./build
)

exec $(dirname "$0")/build/http-server "$@"
