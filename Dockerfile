FROM ubuntu:18.04

COPY src /server/src
COPY test /server/test
COPY external /server/external
COPY CMakeLists.txt* installDependencies.sh /server/
COPY exampleConfig /config

WORKDIR /server

EXPOSE 7007

# Dependencies
RUN apt update && apt install -y sudo git build-essential cmake g++-8
ENV CXX=g++-8
RUN ./installDependencies.sh

# Build server
RUN mkdir build
RUN cd build && cmake -DCMAKE_BUILD_TYPE=Release ..
RUN cd build && make -j$(nproc)

ENTRYPOINT ["/server/build/src/server017" "--config-charset" "/config/characters.json" "--config-match" "/config/matchconfig.match" "--config-scenario" "/config/scenario.scenario" "--port" "7007"]
