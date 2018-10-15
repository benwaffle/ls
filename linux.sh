#!/bin/bash

export CFLAGS="-DLINUX -D_POSIX_C_SOURCE=200809L -DLIBBSD_OVERLAY -isystem /usr/include/bsd -include sys/sysmacros.h"
export LDLIBS="-lbsd"
make
