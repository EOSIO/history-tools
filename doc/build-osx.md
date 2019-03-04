# Install SpiderMonkey 64.0

```
brew install autoconf@2.13 llvm@7 rust
cd ~
wget https://archive.mozilla.org/pub/firefox/releases/64.0/source/firefox-64.0.source.tar.xz
tar xf firefox-64.0.source.tar.xz
cd firefox-64.0/js/src/
autoconf213
```

## SpiderMonkey Release build

```
mkdir build_REL.OBJ
cd build_REL.OBJ
export LIBCLANG_PATH=/usr/local/opt/llvm@7/lib
../configure --disable-debug --enable-optimize
make -j
make install
```

## SpiderMonkey Debug build

```
mkdir build_DBG.OBJ
cd build_DBG.OBJ
export LIBCLANG_PATH=/usr/local/opt/llvm@7/lib
../configure --enable-debug --disable-optimize
make -j
make install
```

# Build wasm-ql

## Release build

```
cd ~
git clone --recursive git@github.com:EOSIO/wasm-api.git
cd wasm-api
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cp ~/firefox-64.0/js/src/build_REL.OBJ/dist/bin/libmozglue.dylib .
make -j
../build-test
brew install node
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
cmake .. -DCMAKE_BUILD_TYPE=Debug
cp ~/firefox-64.0/js/src/build_DBG.OBJ/dist/bin/libmozglue.dylib .
make -j
../build-test
brew install node
cd ../src
npm install node-fetch
```
