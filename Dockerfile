FROM ubuntu:20.04 as base
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get upgrade -y && \
    apt-get install openssl libssl-dev libpq-dev libpqxx-dev build-essential cmake libboost-all-dev git \
    autoconf2.13 libgmp-dev curl libcurl4-openssl-dev python3-pkgconfig ninja-build -y && \
    rm -rf /var/cache/apt/lists

FROM base as builder
WORKDIR /root
RUN mkdir /root/history-tools
COPY cmake /root/history-tools/cmake
COPY CMakeLists.txt /root/history-tools
COPY libraries /root/history-tools/libraries
COPY src /root/history-tools/src
COPY unittests /root/history-tools/unittests
RUN mkdir /root/history-tools/build
WORKDIR /root/history-tools/build
RUN cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DPostgreSQL_TYPE_INCLUDE_DIR=/usr/include/postgresql ..
RUN ninja && ctest --output-on-failure

FROM ubuntu:20.04
RUN apt-get update && apt-get install -y \
    postgresql-client netcat openssl && \
    rm -rf /var/cache/apt/lists

COPY --from=builder /root/history-tools/build/fill-pg /usr/local/bin/fill-pg
