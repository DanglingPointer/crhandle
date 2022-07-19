#!/usr/bin/env bash

mkdir -p cmake-build-manual
cd cmake-build-manual || exit
if test "$1" = "--clean"; then
    rm -rf *;
fi

cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -Dcrhandle_build_tests=ON \
  -DCMAKE_CXX_COMPILER=clang++-12
cmake --build . && ctest --output-on-failure
