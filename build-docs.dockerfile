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
run git clone https://github.com/EOSIO/eosio.cdt.git
workdir /root/eosio.cdt
run git checkout v1.6.1
run git submodule update --init --recursive
run mkdir build
workdir /root/eosio.cdt/build
run cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=/usr/local/eosio.cdt
run ninja
run ninja install

# Hack in headers that standardese needs
run cp -a /root/eosio.cdt/eosio_llvm/tools/clang/include/clang* /root/eosio.cdt/eosio_llvm/include

# custom-built boost isn't compatable with standardese
run rm -rf /usr/local/include/boost /usr/local/lib/libboost*
run apt-get install -y libboost-all-dev

# standardese
workdir /root
run git clone https://github.com/foonathan/standardese.git
workdir /root/standardese
run git checkout 296b92ee52f0a03b6a9b5f52dbee2decaef4a545
run mkdir /root/standardese/build
workdir /root/standardese/build
run cmake -GNinja -DSTANDARDESE_BUILD_TEST=off -DLLVM_CONFIG_BINARY=/root/eosio.cdt/build/eosio_llvm/bin/llvm-config -DCMAKE_BUILD_TYPE=Debug ..
run ninja
run cp -a tool/standardese /usr/local/bin

# gitbook
run npm i -g gitbook-cli
run npm i -g jsdoc-to-markdown

# generate docs. The user may change the BUILD_DATE arg to force a fresh pull
workdir /root
run git clone --recursive https://github.com/EOSIO/history-tools.git
workdir /root/history-tools
arg BUILD_DATE=unknown
run echo ${BUILD_DATE}
run git pull
run bash generate-doc

# result is in /root/history-tools/_book
run ls -l /root/history-tools/_book
