from eosio/history-tools:builder_base as builder

workdir /root
run mkdir /root/history-tools
copy cmake /root/history-tools/cmake
copy CMakeLists.txt /root/history-tools
copy external /root/history-tools/external
copy libraries /root/history-tools/libraries
copy src /root/history-tools/src
copy wasms /root/history-tools/wasms
copy testnet.template /root/history-tools
copy tests /root/history-tools/tests
copy unittests /root/history-tools/unittests

run mkdir /root/history-tools/build
workdir /root/history-tools/build
run cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DSKIP_SUBMODULE_CHECK=1 -DCMAKE_CXX_COMPILER=clang++-8 -DCMAKE_C_COMPILER=clang-8 ..
run ninja && ctest -R "abieos_sql_converter_tests" --output-on-failure

from ubuntu:18.04 
run apt-get update && apt-get install -y \
    postgresql-client netcat openssl libatomic1 libssl1.0

COPY --from=builder /root/history-tools/build/fill-pg /usr/local/bin/fill-pg
