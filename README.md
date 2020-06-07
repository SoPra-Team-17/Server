# Server
This repository contains the server component of team 17 for the 
*No Time To Spy* game of the Softwaregrundprojekt 2019/2020 at 
the university of Ulm.

## Installation 
This server can be installed manually and through a docker container. 

### Manual Installation

#### Prerequisites 
 * C++17 compatible Compiler (e.g. GCC-8)
 * CMake (at least version 3.10)
 * GNU-Make
 * [spdlog](https://github.com/gabime/spdlog/)
 * [CLI11](https://github.com/CLIUtils/CLI11)
 * [afsm](https://github.com/zmij/afsm)
 * [metapushkin](https://github.com/zmij/metapushkin) (dependency from afsm)

#### Compiling the application
Create a directory for the build and change into this. The name of this 
directory can be arbitrarily chosen, in this example it will be called *build*. 
```
mkdir build && cd build
```
Generate a makefile using cmake. This step shouldn't produce any errors if 
all prerequisites are installed. 
```
cmake ..
```
Compile the application using make.
```
make
```
Test the installation by executing the server.
```
./server017 -h
```

## Usage
The standard defines several flags for the server startup:
* `-h` / `--help` prints some help information and exits.
* `-c <path>` / `--config-charset <path>` usage of the given character configuration.
* `-m <path>` / `--config-match <path>` usage of the given match configuration.
* `-s <path>` / `--config-scenario <path>` usage of the given scenario configuration.
* `--x <key> <value>` can be used to give the server additional key-value pairs
* `-v <int>` / `--verbosity <int>` configuration of the logging verbosity
* `-p <int>` / `--port <int>` configuration of the port to be used

## Docker
### Building the docker container
```bash
docker build -t soprateam17/server .
```

### Running the container
```bash
docker run --rm -p 7007:7007 soprateam17/server
```
To get the latest `develop` branch you can also pull the container from [docker hub](https://hub.docker.com/repository/docker/soprateam17/server):
```bash
docker pull soprateam17/server
```

### Customizing configuration files with docker
Configuration can be changed by bindmounting a directory with configuration files to `/config`:
```bash
docker run -v ~/customConfig:/config -p 7007:7007 soprateam17/server
```
With the local configuration files
```
~/customConfig/characters.json
~/customConfig/matchconfig.match
~/customConfig/scenario.scenario
```
The local directory path has to begin with `/` or `~`, so if you have a config directory in the current path use
```bash
sudo docker run --rm -v "$(pwd)"/myConfig:/config -p 7007:7007 soprateam17/server
```
