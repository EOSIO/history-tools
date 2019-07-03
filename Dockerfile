# This Dockerfile is unsupported. Please do not create any issues about Docker.

from ubuntu:19.04

run apt-get -y update
run apt-get -y install build-essential cmake libboost-all-dev git libssl-dev libpq-dev libpqxx-dev autoconf2.13 rustc cargo clang-7 nodejs npm ninja-build libgmp-dev liblmdb-dev
run apt-get -y install wget

workdir /root
run pwd
run wget https://archive.mozilla.org/pub/firefox/releases/64.0/source/firefox-64.0.source.tar.xz
run tar xf firefox-64.0.source.tar.xz
workdir /root/firefox-64.0/js/src/
run autoconf2.13

run mkdir build_REL.OBJ
workdir /root/firefox-64.0/js/src/build_REL.OBJ
env SHELL /bin/bash
run ../configure --disable-debug --enable-optimize --disable-jemalloc --disable-replace-malloc
run make -j 10
run make install

workdir /root
run git clone --recursive https://github.com/EOSIO/eosio.cdt.git
run mkdir /root/eosio.cdt/build
workdir /root/eosio.cdt/build
run cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=/usr/local/eosio.cdt
run cmake --build .
run ninja install

ENV PATH="/usr/local/eosio.cdt/bin:${PATH}"

copy . /root/history-tools
run mkdir /root/history-tools/build
workdir /root/history-tools/build
run apt-get -y install libz-dev
run cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DBOOST_ROOT=/usr
run ninja
