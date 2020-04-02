#!/usr/bin/env bash

# Setup
# Exit the script if any command fails
set -e
# Update apt
sudo apt update
# GCC 8
sudo apt install gcc-8 g++-8
export CC=gcc-8
export CXX=g++-8

# CLI11
cd /tmp
git clone https://github.com/CLIUtils/CLI11.git
cd CLI11
mkdir build && cd build
cmake -DCLI11_BUILD_TESTS=false -DCLI11_BUILD_DOCS=false -DCLI11_BUILD_EXAMPLES=false ..
make -j$(nproc)
sudo make install

# spdlog
cd /tmp
git clone https://github.com/gabime/spdlog.git
cd spdlog
mkdir build && cd build
cmake -DSPDLOG_BUILD_TESTS=false -DSPDLOG_BUILD_EXAMPLE=false ..
make -j$(nproc)
sudo make install


sudo ldconfig
