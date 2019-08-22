from ubuntu:18.04 as builder

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
run wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.gz
run tar xf boost_1_70_0.tar.gz
workdir /root/boost_1_70_0
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
run wget https://github.com/EOSIO/eos/releases/download/v1.8.1/eosio_1.8.1-1-ubuntu-18.04_amd64.deb
run apt-get install -y ./eosio_1.8.1-1-ubuntu-18.04_amd64.deb

workdir /root
run wget https://github.com/EOSIO/eosio.cdt/releases/download/v1.6.2/eosio.cdt_1.6.2-1-ubuntu-18.04_amd64.deb
run apt-get install -y ./eosio.cdt_1.6.2-1-ubuntu-18.04_amd64.deb

workdir /root
run mkdir /root/history-tools
copy cmake /root/history-tools/cmake
copy CMakeLists.txt /root/history-tools
copy external /root/history-tools/external
copy libraries /root/history-tools/libraries
copy src /root/history-tools/src
copy wasms /root/history-tools/wasms

run mkdir /root/history-tools/build
workdir /root/history-tools/build
run cmake -GNinja -DSKIP_SUBMODULE_CHECK=1 -DCMAKE_CXX_COMPILER=clang++-8 -DCMAKE_C_COMPILER=clang-8 ..
run bash -c "cd ../src && npm install node-fetch"
run ninja

copy demo-gui /root/history-tools/demo-gui
workdir /root/history-tools/demo-gui
run npm i && npm run build

# Final image
from ubuntu:18.04
run apt-get update && apt-get install -y libssl1.0.0 libatomic1
run rm -rf /root/.local/share/eosio/

workdir /root
run mkdir history-tools
workdir /root/history-tools
run mkdir build
run mkdir src
run mkdir -p demo-gui/dist
workdir /root/history-tools/build

copy --from=builder /root/history-tools/src/query-config.json /root/history-tools/src/
copy --from=builder /root/history-tools/build/combo-rocksdb /root/history-tools/build/
copy --from=builder /root/history-tools/build/fill-rocksdb /root/history-tools/build/
copy --from=builder /root/history-tools/build/wasm-ql-rocksdb /root/history-tools/build/
copy --from=builder /root/history-tools/build/chain-server.wasm /root/history-tools/build/
copy --from=builder /root/history-tools/build/legacy-server.wasm /root/history-tools/build/
copy --from=builder /root/history-tools/build/token-server.wasm /root/history-tools/build/
copy --from=builder /root/history-tools/demo-gui/dist/chain-client.wasm /root/history-tools/demo-gui/dist/
copy --from=builder /root/history-tools/demo-gui/dist/client.bundle.js /root/history-tools/demo-gui/dist/
copy --from=builder /root/history-tools/demo-gui/dist/index.html /root/history-tools/demo-gui/dist/
copy --from=builder /root/history-tools/demo-gui/dist/token-client.wasm /root/history-tools/demo-gui/dist/

workdir /root/history-tools/build
expose 80/tcp
entrypoint ["./combo-rocksdb", "--wql-static-dir", "../demo-gui/dist/", "--wql-listen", "0.0.0.0:80"]
