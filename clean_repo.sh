#!/bin/bash

# Ensure we're at the repo root
cd "$(git rev-parse --show-toplevel)" || exit 1

echo "Cleaning build directories..."
rm -rf build/* install/*

echo "Cleaning submodules..."
git submodule deinit -f stf_lib riscv-tests
rm -rf .git/modules/stf_lib .git/modules/riscv-tests
git submodule update --init --recursive

#echo "Recreating build and install directories..."
#mkdir -p build install
#cd build
#
#echo "Reconfiguring and rebuilding..."
#../configure --prefix="$(pwd)/../install"
#make -j$(nproc)
#
#echo "Rebuild complete!"
#
