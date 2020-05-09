FROM ubuntu:18.04

COPY . /server
COPY exampleConfig /config

WORKDIR /server

EXPOSE 7007

# Dependencies
RUN apt update && apt install -y sudo git build-essential cmake g++-8
ENV CXX=g++-8
RUN ./installDependencies.sh

# Build server
RUN mkdir build && cd build
RUN cd build && cmake -DCMAKE_BUILD_TYPE=Release ..
RUN cd build && make -j$(nproc)

# Remove build files
RUN mv /server/build/server017 /server017
RUN rm -rf /server

ENTRYPOINT /server017 --config-charset /config/characters.json --config-match /config/matchconfig.json --config-scenario /config/scenario.json --port 7007
