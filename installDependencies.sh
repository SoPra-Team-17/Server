#!/usr/bin/env bash

# Setup
# Exit the script if any command fails
set -e

# Dependencies from LibCommon
./external/LibCommon/installDependencies.sh

# Dependencies from WebsocketCPP
./external/WebsocketCPP/installDependencies.sh

# afsm
cd /tmp
git clone --depth 1 https://github.com/zmij/afsm.git
cd afsm
mkdir build && cd build
cmake -DAFSM_BUILD_TESTS=false ..
make -j$(nproc)
sudo make install

# metapushkin (asfm dependency)
cd /tmp
git clone --depth 1 https://github.com/zmij/metapushkin.git
cd metapushkin
mkdir build && cd build
cmake -DMETA_BUILD_TESTS=false ..
make -j$(nproc)
sudo make install

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
