#!/usr/bin/env bash

# Setup
# Exit the script if any command fails
set -e

# CLI11
cd /tmp
git clone https://github.com/CLIUtils/CLI11.git
cd CLI11
mkdir build && cd build
cmake -DCLI11_BUILD_TESTS=false ..
make -j$(nproc)
sudo make install

# spdlog
cd /tmp
git clone https://github.com/gabime/spdlog.git
cd spdlog
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install


sudo ldconfig
