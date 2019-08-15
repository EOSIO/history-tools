# Ubuntu Server 18.04 instructions

These instructions assume:
* You are using a minimal Ubuntu Server 18.04 image; other versions may not work
* You are using a fresh image or container. If you installed any packages before
  following these instructions, or selected any optional components (e.g. compilers,
  services, GUIs, etc.), then these instructions may fail.

The repo also includes a Dockerfile which does these steps. The Dockerfile needs
10 cores and plenty of RAM to build.

# Install build environment

Install clang 8 and other needed tools:
```
apt update && apt install -y wget gnupg

cd ~
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -

cat <<EOT >>/etc/apt/sources.list
deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic main
deb-src http://apt.llvm.org/bionic/ llvm-toolchain-bionic main
deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main
deb-src http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main
EOT

apt update && apt install -y \
    autoconf2.13        \
    build-essential     \
    bzip2               \
    cargo               \
    clang-8             \
    git                 \
    libgmp-dev          \
    libpq-dev           \
    lld-8               \
    lldb-8              \
    ninja-build         \
    nodejs              \
    npm                 \
    pkg-config          \
    postgresql-server-dev-all \
    python2.7-dev       \
    python3-dev         \
    rustc               \
    zlib1g-dev

update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100
update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100
```

Build and install Boost 1.70. Adjust `-j10` to match your machine. Bad things will
happen if you don't have enough RAM for the number of cores you use:

```
cd ~
wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.gz
tar xf boost_1_70_0.tar.gz
cd boost_1_70_0
./bootstrap.sh
./b2 toolset=clang -j10 install
```

Build and install CMake 3.14.5. Adjust `--parallel=` and `-j` to match your machine.
Bad things will happen if you don't have enough RAM for the number of cores you use:

```
cd ~
wget https://github.com/Kitware/CMake/releases/download/v3.14.5/cmake-3.14.5.tar.gz
tar xf cmake-3.14.5.tar.gz
cd cmake-3.14.5
./bootstrap --parallel=10
make -j10
make -j10 install
```

# Install eosio 1.8.1

```
cd ~
wget https://github.com/EOSIO/eos/releases/download/v1.8.1/eosio_1.8.1-1-ubuntu-18.04_amd64.deb
apt install -y ./eosio_1.8.1-1-ubuntu-18.04_amd64.deb
```

# Install CDT 1.6.1

```
cd ~
wget https://github.com/EOSIO/eosio.cdt/releases/download/v1.6.1/eosio.cdt_1.6.1-1_amd64.deb
apt install -y ./eosio.cdt_1.6.1-1_amd64.deb
```

# Build History Tools

```
cd ~
git clone --recursive https://github.com/EOSIO/history-tools.git
cd history-tools
mkdir build
cd build
cmake -GNinja -DCMAKE_CXX_COMPILER=clang++-8 -DCMAKE_C_COMPILER=clang-8 ..
bash -c "cd ../src && npm install node-fetch"
ninja
```
