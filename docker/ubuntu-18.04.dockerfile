FROM ubuntu:18.04
# install ppa dependencies
RUN apt-get update && \
    apt-get install -yq \
        binutils-gold \
        build-essential \
        clang-tools-8 \
        curl \
        g++-8 \
        git \
        libcurl4-gnutls-dev \
        libgmp3-dev \
        libssl-dev \
        libusb-1.0-0-dev \
        lld-8 \
        llvm-7 \
        llvm-7-dev \
        locales \
        ninja-build \
        pkg-config \
        python \
        software-properties-common \
        wget \
        xz-utils \
        zlib1g-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
# configure build environment
RUN update-alternatives --remove-all cc && \
    update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-8 100 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 100 && \
    update-alternatives --remove-all c++ && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-8 100 && \
    update-alternatives --install /usr/bin/gcc++ gcc++ /usr/bin/g++-8 100 && \
    update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-8 100 && \
    locale-gen en_US.UTF-8
ENV LANG='en_US.UTF-8'
# CMake
RUN curl -LO https://cmake.org/files/v3.13/cmake-3.13.2.tar.gz && \
    tar -xzf cmake-3.13.2.tar.gz && \
    cd cmake-3.13.2 && \
    ./bootstrap --prefix=/usr/local --parallel=$(nproc) && \
    make -j $(nproc) && \
    make install && \
    cd .. && \
    rm -rf cmake-3.13.2.tar.gz cmake-3.13.2
# boost
RUN curl -LO https://dl.bintray.com/boostorg/release/1.72.0/source/boost_1_72_0.tar.bz2 && \
    tar -xjf boost_1_72_0.tar.bz2 && \
    cd boost_1_72_0 && \
    ./bootstrap.sh --prefix=/usr/local && \
    ./b2 --with-iostreams --with-date_time --with-filesystem --with-system --with-program_options --with-chrono --with-test -j $(nproc) install && \
    cd .. && \
    rm -rf boost_1_72_0.tar.bz2 boost_1_72_0
# eosio.cdt
RUN git clone https://github.com/EOSIO/eosio.cdt.git && \
    cd eosio.cdt && \
    git checkout eosio-cdt-2.1-staging-b && \
    git submodule update --init --recursive
RUN mkdir -p eosio.cdt/build && \
    cd eosio.cdt/build && \
    CC=clang-8 CXX=clang++-8 cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_AR=/usr/bin/llvm-ar-8 -DCMAKE_RANLIB=/usr/bin/llvm-ranlib-8 -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld .. && \
    CC=clang-8 CXX=clang++-8 ninja