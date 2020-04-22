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