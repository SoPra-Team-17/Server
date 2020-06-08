FROM ubuntu:18.04

# Dependencies
RUN apt update && apt install -y sudo git build-essential cmake g++-8
COPY external /server/external
COPY installDependencies.sh /server/
ENV CXX=g++-8
WORKDIR /server
RUN ./installDependencies.sh

COPY src /server/src
COPY test /server/test
COPY CMakeLists.txt* /server/
COPY exampleConfig /config

# Build server
RUN mkdir build
RUN cd build && cmake -DCMAKE_BUILD_TYPE=Release ..
RUN cd build && make -j$(nproc)

EXPOSE 7007

ENTRYPOINT ["/server/build/src/server017", "--config-charset", "/config/characters.json", "--config-match", "/config/matchconfig.match", "--config-scenario", "/config/scenario.scenario", "--port", "7007"]
