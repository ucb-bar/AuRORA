#!/usr/bin/env bash

sed -i "s/li a1, 1/li a1, 2/g" riscv-tests/benchmarks/common/crt.S

cd build
rm -rf bareMetalC/mt*
make bareMetalC
