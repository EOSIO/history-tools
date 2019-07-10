# OSX Mojave instructions

These instructions assume:
* You are using OSX Mojave; prior versions won't work
* You installed brew
* You installed xcode's command-line tools and they're up to date

There may be more brew packages needed.

# Install build environment

Install clang 8 and other needed tools:
```
brew install llvm@8 cmake ninja git boost autoconf@2.13 rust node
```

# Build SpiderMonkey 64.0 (optional)

You can skip this if you're not building wasm-ql

```
cd ~
wget https://archive.mozilla.org/pub/firefox/releases/64.0/source/firefox-64.0.source.tar.xz
tar xf firefox-64.0.source.tar.xz
cd firefox-64.0/js/src/
autoconf213
```

Choose 1 of the following 2 options. Release runs much faster, but isn't compatible with debug builds of the history tools. Debug isn't compatible with release builds of the history tools.

## SpiderMonkey Release option

```
mkdir build_REL.OBJ
cd build_REL.OBJ
export LIBCLANG_PATH=/usr/local/opt/llvm/lib
../configure --disable-debug --enable-optimize --disable-jemalloc --disable-replace-malloc
make -j
make install
```

## SpiderMonkey Debug option

```
mkdir build_DBG.OBJ
cd build_DBG.OBJ
export LIBCLANG_PATH=/usr/local/opt/llvm/lib
../configure --enable-debug --disable-optimize --disable-jemalloc --disable-replace-malloc
make -j
make install
```

# Build History Tools

Note: ignore warnings which look like this:

```
ld: warning: direct access in function ... to global weak symbol ... from file ... means the weak symbol cannot be overridden at runtime. This was likely caused by different translation units being compiled with different visibility settings.
```

## Release build

```
cd ~
git clone --recursive https://github.com/EOSIO/history-tools.git
cd history-tools
mkdir build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
cp ~/firefox-64.0/js/src/build_REL.OBJ/dist/bin/libmozglue.dylib .
ninja
bash -c "cd ../src && npm install node-fetch"
```

## Debug build

```
cd ~
git clone --recursive https://github.com/EOSIO/history-tools.git
cd history-tools
mkdir build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
cp ~/firefox-64.0/js/src/build_DBG.OBJ/dist/bin/libmozglue.dylib .
ninja
bash -c "cd ../src && npm install node-fetch"
```
