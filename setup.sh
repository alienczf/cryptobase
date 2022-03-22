#!/usr/bin/env bash

SOURCE="${BASH_SOURCE[0]}"
DIR_REL=""
while [ -h "$SOURCE" ] ; do SOURCE="$(readlink "$SOURCE")"; done
DIR_ROOT="$( cd -P "$( dirname "$SOURCE" )$DIR_REL" && pwd )"

# init ext repos
git submodule update --recursive --init

export CXX=clang++
export CC=clang

echo "print CMAKE_FLAGS $CMAKE_FLAGS"

# Generate release config first
CC_RELEASE_BIN="${DIR_ROOT}/_build/r"
mkdir -p "${CC_RELEASE_BIN}"
pushd "${CC_RELEASE_BIN}"
touch CMakeCache.txt && cmake -DPYBIND11_PYTHON_VERSION=3.7 -DCMAKE_BUILD_TYPE=Release ${CMAKE_FLAGS} -GNinja ../..
popd

CC_DEBUG_BIN="${DIR_ROOT}/_build/d"
mkdir -p "${CC_DEBUG_BIN}"
pushd "${CC_DEBUG_BIN}"
  touch CMakeCache.txt && cmake -DPYBIND11_PYTHON_VERSION=3.7 -DCMAKE_BUILD_TYPE=Debug ${CMAKE_FLAGS} -GNinja ../..
popd

echo "Done, go build something nice!"
exit 0
