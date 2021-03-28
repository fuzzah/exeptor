#!/bin/bash

rm -fr ./bld
mkdir bld && cd bld

cmake .. && cmake --build .

