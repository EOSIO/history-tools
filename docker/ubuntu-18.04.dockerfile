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