# Server
This repository contains the server component of team 17 for the 
*No Time To Spy* game of the Softwaregrundprojekt 2019/2020 at 
the university of Ulm.

## Installation 
Currently this server can be only installed manually, later on there will
be also the option to install it through a docker container. 

### Manual Installation

#### Prerequisites 
 * C++17 compatible Compiler (e.g. GCC-8)
 * CMake (at least version 3.10)
 * GNU-Make

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
./server017
```

## Usage
The standard defines several flags for the server startup:
* `-h` / `--help` prints some help information and exits.
* `-c <path>` / `--config-charset <path>` configuration of the charset.
* `-m <path>` / `--config-match <path>` usage of the given match configuration.
* `-s <path>` / `--config-scenario <path>` usage of the given scenario configuration.
* `--x <key> <value>` can be used to give the server additional key-value pairs
* `-v <int>` / `--verbosity <int>` configuration of the logging verbosity
* `-p <int>` / `--port <int>` configuration of the port to be used