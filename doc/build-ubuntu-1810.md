# Preliminaries

```
sudo apt update
sudo apt upgrade
sudo apt install build-essential cmake libboost-all-dev git libssl-dev libpq-dev libpqxx-dev autoconf2.13 rustc cargo clang-7 nodejs npm ninja-build libgmp-dev liblmdb-dev
```

# Build SpiderMonkey 64.0

You can skip this if you're not building wasm-ql

```
cd ~
wget https://archive.mozilla.org/pub/firefox/releases/64.0/source/firefox-64.0.source.tar.xz
tar xf firefox-64.0.source.tar.xz
cd firefox-64.0/js/src/
autoconf2.13
```

## SpiderMonkey Release build

```
mkdir build_REL.OBJ
cd build_REL.OBJ
../configure --disable-debug --enable-optimize --disable-jemalloc --disable-replace-malloc
make -j
sudo make install
```

## SpiderMonkey Debug build

```
mkdir build_DBG.OBJ
cd build_DBG.OBJ
../configure --enable-debug --disable-optimize --disable-jemalloc --disable-replace-malloc
make -j
sudo make install
```

# Build History Tools

## Release build

```
cd ~
git clone --recursive git@github.com:EOSIO/wasm-api.git
cd wasm-api
mkdir build
cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja
../build-test
export LD_LIBRARY_PATH=/usr/local/lib/:$LD_LIBRARY_PATH
cd ../src
npm install node-fetch
```

## Debug build

```
cd ~
git clone --recursive git@github.com:EOSIO/wasm-api.git
cd wasm-api
mkdir build
cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug
ninja
../build-test
export LD_LIBRARY_PATH=/usr/local/lib/:$LD_LIBRARY_PATH
cd ../src
npm install node-fetch
```
