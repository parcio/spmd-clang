# SPMDClang

A Clang plugin for correctness checking of MPI programs.

## Installation

There are currently two ways to install SPMDClang:

1. Using the provided Dockerfile
2. Manually building the plugin

### Docker

Run the following commands with current working directory being the root of this repository.
Currently building the Docker image takes some time because it has to compile llvm-22 from source (will be fixed in future releases).
You can also pull the image from Docker Hub.

```bash
# EITHER: Build the docker image
docker build -f Dockerfile.spmdclang -t spmd-clang:latest .
# OR: Pull the image from Docker Hub
docker pull mblesel/spmdclang:latest

# Run the docker image
docker run -it spmd-clang:latest

# Inside the docker image
cd /spmd-clang/tests
./test-valid.sh # To run the tests and confirm that it works

# Load the plugin environment in the container to get access to the spmd-clang compiler wrapper command
. /spmd-clang/environment-docker.sh
# to compile a .c file:
spmd-clang <filename>.c # try it out in the tests directory to confirm that the command works
```

### Manual Installation

There two main dependencies for SPMDClang:

1. llvm-22 (older versions might not be able to compile the plugin due to API changes).
   This can either be a manual installation of llvm or one via the system package manager.
2. MPICH as MPI implementation (support for other MPI implementations is planned for future releases).

To manually build the plugin, edit the `build_script.sh` and set the correct `LLVM_INSTALL_DIR` path.
Also set the `LLVM_PATH` in the `environment.sh` script to the correct path.
After that run the following in the root of this repository:

```bash
# build the plugin
./build_script.sh
```

This will generate the Clang plugin at `./build/spmd-plugin/lib/libSPMDPlugin.so`.

## Usage

To simply use SPMDClang on individual C code files the `environment.sh` script provides a wrapper command `spmd-clang`.

First source the environment script:

```bash
source ./environment.sh
```

Then compile a C file with the plugin:

```bash
spmd-clang <filename>.c
```

SPMDClang can be configured via a `.spmdclang` file in the current working directory.
see tests/.spmdclang for an example:

```bash
# .spmdclang file
PROCESS_COUNT=3
```

Currently the only configurable parameter is the number of MPI processes that should be used for the communication scheme analysis.

## Publications

Blesel, Michael; Kuhn, Michael; Fangohr, Hand (2026): [Compile Time Correctness Checking of MPI Communication Schemes with Clang Using Coloured Petri Nets](https://dl.gi.de/items/1e221b9b-9ffd-4258-90be-ea5eecbcd148). PARS-Mitteilungen: Vol. 38, Nr. 3. Gesellschaft für Informatik e.V., Fachgruppe PARS. ISSN: 0177-0454
