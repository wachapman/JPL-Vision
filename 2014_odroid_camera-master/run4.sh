#!/bin/bash
export LD_PRELOAD=/usr/lib/arm-linux-gnueabihf/libv4l/v4l1compat.so
#valgrind --leak-check=yes \
#gdb --args \
./capture4
