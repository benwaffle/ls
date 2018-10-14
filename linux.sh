#!/bin/bash

export CFLAGS="-DLINUX -D_POSIX_C_SOURCE=200809L -include bsd/string.h"
export LDLIBS="-lbsd"
make
