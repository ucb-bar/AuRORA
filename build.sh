#!/usr/bin/env bash

git submodule init
git submodule update

cd riscv-tests
git submodule init
git submodule update
cd ../

if [ ! -d "build" ] ; then
    autoconf && \
        mkdir build && cd build && \
        ../configure &&
        cd ..

    if [ $? -ne 0 ] ; then
        echo $0 failed
        exit 1
    fi
fi

cd build

if [[ $(which riscv64-unknown-linux-gnu-gcc) ]] ; then
    make -j $@
else
    make -j BAREMETAL_ONLY=1 $@
fi

