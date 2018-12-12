# copyright defined in LICENSE.txt

cmake_minimum_required (VERSION 3.11)
project(wasm-api VERSION 0.1 LANGUAGES CXX)

set(default_build_type "Release")
 
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

FIND_PACKAGE(Boost 1.58 REQUIRED COMPONENTS system program_options)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(wasm-api src/wasm-api.cpp)
target_include_directories(wasm-api PUBLIC external/abieos/src external/abieos/external/rapidjson/include PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(wasm-api Boost::system Boost::program_options mozjs-63 pqxx pq -lpthread)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wasm-api PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics -include /usr/local/include/mozjs-63/js/RequiredDefines.h -I/usr/local/include/mozjs-63)
endif()
