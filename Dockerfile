from ubuntu:18.04

workdir /root
run apt-get update && apt-get install -y wget gnupg
run wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -

run echo '\n\
deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic main\n\
deb-src http://apt.llvm.org/bionic/ llvm-toolchain-bionic main\n\
deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main\n\
deb-src http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main\n' >>/etc/apt/sources.list

run apt-get update && apt-get install -y \
    autoconf2.13                \
    build-essential             \
    bzip2                       \
    cargo                       \
    clang-8                     \
    git                         \
    libgmp-dev                  \
    liblmdb-dev                 \
    libpq-dev                   \
    lld-8                       \
    lldb-8                      \
    ninja-build                 \
    nodejs                      \
    npm                         \
    pkg-config                  \
    postgresql-server-dev-all   \
    python2.7-dev               \
    python3-dev                 \
    rustc                       \
    zlib1g-dev

run update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100
run update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100

workdir /root
run wget https://dl.bintray.com/boostorg/release/1.69.0/source/boost_1_69_0.tar.gz
run tar xf boost_1_69_0.tar.gz
workdir /root/boost_1_69_0
run ./bootstrap.sh
run ./b2 toolset=clang -j10 install

workdir /root
run wget https://github.com/Kitware/CMake/releases/download/v3.14.5/cmake-3.14.5.tar.gz
run tar xf cmake-3.14.5.tar.gz
workdir /root/cmake-3.14.5
run ./bootstrap --parallel=10
run make -j10
run make -j10 install

workdir /root
run wget https://archive.mozilla.org/pub/firefox/releases/64.0/source/firefox-64.0.source.tar.xz
run tar xf firefox-64.0.source.tar.xz
workdir /root/firefox-64.0/js/src/
run autoconf2.13

run mkdir build_REL.OBJ
workdir /root/firefox-64.0/js/src/build_REL.OBJ
run SHELL=/bin/bash ../configure --disable-debug --enable-optimize --disable-jemalloc --disable-replace-malloc
run SHELL=/bin/bash make -j10
run SHELL=/bin/bash make install

workdir /root
run wget https://github.com/EOSIO/eos/releases/download/v1.8.0/eosio_1.8.0-1-ubuntu-18.04_amd64.deb
run apt-get install -y ./eosio_1.8.0-1-ubuntu-18.04_amd64.deb

workdir /root
run wget https://github.com/EOSIO/eosio.cdt/releases/download/v1.6.1/eosio.cdt_1.6.1-1_amd64.deb
run apt-get install -y ./eosio.cdt_1.6.1-1_amd64.deb

workdir /root
run git clone --recursive https://github.com/EOSIO/history-tools.git
run mkdir /root/history-tools/build
workdir /root/history-tools/build
run cmake -GNinja -DCMAKE_CXX_COMPILER=clang++-8 -DCMAKE_C_COMPILER=clang-8 ..
run bash -c "cd ../src && npm install node-fetch"
run ninja
