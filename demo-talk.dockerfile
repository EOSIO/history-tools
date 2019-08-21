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

run apt-get update && apt-get install -y psmisc
copy demo-gui /root/history-tools/demo-gui
copy demo-talk /root/history-tools/demo-talk
workdir /root/history-tools/demo-talk/src

run eosio-cpp                                                       \
    -I ../../libraries/eosiolib/wasmql                              \
    -I ../../external/abieos/external/date/include                  \
    talk.cpp

run eosio-cpp                                                       \
    -Os                                                             \
    -D WASM_QL                                                      \
    -I ../../libraries/eosiolib/wasmql                              \
    -I ../../external/abieos/external/date/include                  \
    ../../libraries/eosiolib/wasmql/eosio/temp_placeholders.cpp     \
    -fquery-server                                                  \
    --eosio-imports=../../libraries/eosiolib/wasmql/server.imports  \
    talk-server.cpp                                                 \
    -o talk-server.wasm

run eosio-cpp                                                       \
    -Os                                                             \
    -D WASM_QL                                                      \
    -I ../../libraries/eosiolib/wasmql                              \
    -I ../../external/abieos/external/date/include                  \
    ../../libraries/eosiolib/wasmql/eosio/temp_placeholders.cpp     \
    -fquery-client                                                  \
    --eosio-imports=../../libraries/eosiolib/wasmql/client.imports  \
    talk-client.cpp                                                 \
    -o talk-client.wasm

workdir /root/history-tools/demo-talk
run npm i -g yarn
run yarn && yarn build

workdir /root/history-tools/demo-talk/src
run \
    cleos wallet create --to-console | tail -n 1 | sed 's/"//g' >/password                      && \
    cleos wallet import --private-key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3       && \
    (  nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::state_history_plugin    \
       --disable-replay-opts --chain-state-history --trace-history                                 \
       -d /nodeos-data --config-dir /nodeos-config & )                                          && \
    sleep 2                                                                                     && \
    cleos create account eosio talk     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos set code talk talk.wasm                                                               && \
    cleos set abi talk talk.abi                                                                 && \
    cleos create account eosio adam     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio bill     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio bob      EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio jack     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio jane     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio jenn     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio jill     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio joe      EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio sam      EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio sue      EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    sleep 2                                                                                     && \
    killall nodeos                                                                              && \
    tail --pid=`pidof nodeos` -f /dev/null                                                      && \
    rm -rf /nodeos-data/snapshots /nodeos-data/state /nodeos-data/state-history                 && \
    rm -rf /nodeos-data/blocks/reversible

# Final image
from ubuntu:18.04
run apt-get update && apt-get install -y    \
    libssl1.0.0                             \
    libatomic1                              \
    supervisor                              \
    nginx                                   \
    nodejs

workdir /root
copy --from=builder /root/eosio_1.8.1-1-ubuntu-18.04_amd64.deb /root
run apt-get install -y ./eosio_1.8.1-1-ubuntu-18.04_amd64.deb

run mkdir -p /nodeos-data/blocks
copy --from=builder /nodeos-data/blocks/blocks.log /nodeos-data/blocks

run \
    cleos wallet create --to-console | tail -n 1 | sed 's/"//g' >/password                      && \
    cleos wallet import --private-key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3

run chmod 755 /root
run mkdir -p /var/log/supervisor
copy demo-talk/supervisord.conf /etc/supervisor/conf.d/supervisord.conf
copy demo-talk/nginx-site.conf /etc/nginx/sites-available/default

workdir /root
run mkdir history-tools
workdir /root/history-tools
run mkdir -p        \
    build           \
    demo-talk/dist  \
    demo-talk/src   \
    src

copy --from=builder /root/history-tools/build/chain-server.wasm                 /root/history-tools/build/
copy --from=builder /root/history-tools/build/combo-rocksdb                     /root/history-tools/build/
copy --from=builder /root/history-tools/build/fill-rocksdb                      /root/history-tools/build/
copy --from=builder /root/history-tools/build/legacy-server.wasm                /root/history-tools/build/
copy --from=builder /root/history-tools/build/token-server.wasm                 /root/history-tools/build/
copy --from=builder /root/history-tools/build/wasm-ql-rocksdb                   /root/history-tools/build/
copy --from=builder /root/history-tools/demo-talk/src/talk-server.wasm          /root/history-tools/build/

copy --from=builder /root/history-tools/demo-talk/dist/chain-client.wasm        /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/client.bundle.js         /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/data-description.md      /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/index.html               /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/introduction.md          /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/query-description.md     /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/talk-client.cpp          /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/talk-client.wasm         /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/talk-server.cpp          /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/talk.cpp                 /root/history-tools/demo-talk/dist
copy --from=builder /root/history-tools/demo-talk/dist/talk.hpp                 /root/history-tools/demo-talk/dist

copy --from=builder /root/history-tools/demo-talk/src/fill.js                   /root/history-tools/demo-talk/src/

copy --from=builder /root/history-tools/src/query-config.json                   /root/history-tools/src/

workdir /root
expose 80/tcp
cmd ["/usr/bin/supervisord"]
