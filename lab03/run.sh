#!/bin/sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
LD_PRELOAD=./libmaze.so ./maze-server
