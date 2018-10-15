#!/bin/bash

export CFLAGS="-DLINUX -D_POSIX_C_SOURCE=200809L -DLIBBSD_OVERLAY -isystem /usr/include/bsd"
export LDLIBS="-lbsd"
make
