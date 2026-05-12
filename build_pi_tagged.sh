#!/usr/bin/env bash
set -e

cd build-release-arm64 && make

HASH=$(git -C .. rev-parse --short HEAD)
cp capy "capy-${HASH}"
echo "Built: capy-${HASH}"
