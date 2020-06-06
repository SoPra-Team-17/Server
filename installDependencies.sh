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
git clone --depth 1 --branch develop https://github.com/zmij/afsm.git
cd afsm
git fetch origin 49181bf52fa2ab8bcea6017d11b3cd321b56c13c
git checkout 49181bf52fa2ab8bcea6017d11b3cd321b56c13c
mkdir build && cd build
cmake -DAFSM_BUILD_TESTS=false ..
make -j$(nproc)
sudo make install

# metapushkin (asfm dependency)
cd /tmp
git clone --depth 1 --branch develop https://github.com/zmij/metapushkin.git
cd metapushkin
git fetch origin a2fc4725408e7a7cf53e41ced8101d399c9fe387
git checkout a2fc4725408e7a7cf53e41ced8101d399c9fe387
mkdir build && cd build
cmake -DMETA_BUILD_TESTS=false ..
make -j$(nproc)
sudo make install

# CLI11
cd /tmp
git --depth 1 -b v1.9.0 clone https://github.com/CLIUtils/CLI11.git
cd CLI11
mkdir build && cd build
cmake -DCLI11_BUILD_TESTS=false -DCLI11_BUILD_DOCS=false -DCLI11_BUILD_EXAMPLES=false ..
make -j$(nproc)
sudo make install

# spdlog
cd /tmp
git --depth 1 -b v1.6.1 clone https://github.com/gabime/spdlog.git
cd spdlog
mkdir build && cd build
cmake -DSPDLOG_BUILD_TESTS=false -DSPDLOG_BUILD_EXAMPLE=false ..
make -j$(nproc)
sudo make install


sudo ldconfig
