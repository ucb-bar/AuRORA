#!/usr/bin/env bash

sed -i "s/define MULTICORE 1/define MULTICORE 2/g" riscv-tests/benchmarks/common/crt.S

cd build
rm -rf bareMetalC/mt*
make bareMetalC
