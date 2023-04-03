#!/bin/zsh

#
# For Release Build
#

cmake -B build -DBUILD_TESTING=OFF
cmake --build build --config=Release
cp -pf build/host/Clemens\ IIGS.app /Applications

echo Double Click /Applications/Clemens\ IIGS.app to execute
