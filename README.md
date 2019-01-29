# Install SpiderMonkey 64.0 (Ubuntu 18.10)

```
sudo apt update
sudo apt upgrade
sudo apt install build-essential cmake libboost-all-dev git libpq-dev libpqxx-dev autoconf2.13 rustc cargo clang-7
cd ~
wget https://archive.mozilla.org/pub/firefox/releases/64.0/source/firefox-64.0.source.tar.xz
tar xf firefox-64.0.source.tar.xz
cd firefox-64.0/js/src/
autoconf2.13
```

## SpiderMonkey Debug build (Ubuntu 18.10)

```
mkdir build_DBG.OBJ
cd build_DBG.OBJ
../configure --enable-debug --disable-optimize --disable-jemalloc --disable-replace-malloc
make -j
sudo make install
```

## SpiderMonkey Release build (Ubuntu 18.10)

```
mkdir build_REL.OBJ
cd build_REL.OBJ
../configure --disable-debug --enable-optimize --disable-jemalloc --disable-replace-malloc
make -j
sudo make install
```

# Build wasm-ql (Ubuntu 18.10)

## Debug build (Ubuntu 18.10)

```
git clone --recursive git@github.com:EOSIO/wasm-api.git
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j
../build-test
export LD_LIBRARY_PATH=/usr/local/lib/
./wasm-ql -e 0.0.0.0:8880 
curl localhost:8880/v1/chain/get_table_rows -d '{"code":"eosio", "scope":"eosio", "table":"namebids", "show_payer":true, "json":true, "key_type": "name", "index_position": "2", "limit":100}' | json_pp
```

## Release build (Ubuntu 18.10)

```
git clone --recursive git@github.com:EOSIO/wasm-api.git
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
../build-test
export LD_LIBRARY_PATH=/usr/local/lib/
./wasm-ql -e 0.0.0.0:8880 
curl localhost:8880/v1/chain/get_table_rows -d '{"code":"eosio", "scope":"eosio", "table":"namebids", "show_payer":true, "json":true, "key_type": "name", "index_position": "2", "limit":100}' | json_pp
```

# Install SpiderMonkey 64.0 (OSX)

```
brew install autoconf@2.13 llvm@7 rust
cd ~
wget https://archive.mozilla.org/pub/firefox/releases/64.0/source/firefox-64.0.source.tar.xz
tar xf firefox-64.0.source.tar.xz
cd firefox-64.0/js/src/
autoconf213
```

## SpiderMonkey Debug build (OSX)

```
mkdir build_DBG.OBJ
cd build_DBG.OBJ
export LIBCLANG_PATH=/usr/local/opt/llvm@7/lib
../configure --enable-debug --disable-optimize
make -j
make install
```

## SpiderMonkey Release build (OSX)

```
mkdir build_REL.OBJ
cd build_REL.OBJ
export LIBCLANG_PATH=/usr/local/opt/llvm@7/lib
../configure --disable-debug --enable-optimize
make -j
make install
```

# Build wasm-ql (OSX)

## Debug build (OSX)

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cp ~/firefox-64.0/js/src/build_DBG.OBJ/dist/bin/libmozglue.dylib .
make -j
../build-test
```

## Release build (OSX)

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cp ~/firefox-64.0/js/src/build_DBG.OBJ/dist/bin/libmozglue.dylib .
make -j
../build-test
```
