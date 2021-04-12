FROM ubuntu:20.04 as base
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get upgrade -y
RUN apt-get install openssl libssl-dev libpq-dev libpqxx-dev build-essential cmake libboost-all-dev git \
    autoconf2.13 libgmp-dev curl libcurl4-openssl-dev python3-pkgconfig ninja-build -y && \
    rm -rf /var/cache/apt/lists

FROM base as builder
workdir /root
run mkdir /root/history-tools
copy cmake /root/history-tools/cmake
copy CMakeLists.txt /root/history-tools
copy external /root/history-tools/external
copy libraries /root/history-tools/libraries
copy src /root/history-tools/src
copy unittests /root/history-tools/unittests

run mkdir /root/history-tools/build
workdir /root/history-tools/build
run cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DSKIP_SUBMODULE_CHECK=1 -DPostgreSQL_TYPE_INCLUDE_DIR=/usr/include/postgresql ..
run ninja && ctest --output-on-failure

FROM ubuntu:20.04
run apt-get update && apt-get install -y \
    postgresql-client netcat openssl && \
    rm -rf /var/cache/apt/lists

COPY --from=builder /root/history-tools/build/fill-pg /usr/local/bin/fill-pg
