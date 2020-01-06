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
ninja
bash -c "cd ../src && npm install node-fetch"
```
