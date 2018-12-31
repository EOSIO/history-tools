# copyright defined in LICENSE.txt

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)

cmake_minimum_required (VERSION 3.11)
project(wasm-ql VERSION 0.1 LANGUAGES CXX)

set(default_build_type "Release")
 
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

find_package(Boost 1.58 REQUIRED COMPONENTS date_time system program_options)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JS REQUIRED mozjs-64)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(wasm-ql src/wasm-ql.cpp)
target_include_directories(wasm-ql PUBLIC external/abieos/src external/abieos/external/rapidjson/include PRIVATE ${Boost_INCLUDE_DIR})
target_include_directories(wasm-ql PUBLIC ${JS_INCLUDE_DIRS})
target_compile_options(wasm-ql PUBLIC ${JS_CFLAGS_OTHER})
target_link_libraries(wasm-ql Boost::date_time Boost::system Boost::program_options pqxx pq -lpthread)
target_link_libraries(wasm-ql ${JS_LIBRARIES})

string(TOLOWER ${CMAKE_BUILD_TYPE} LOWERCASE_CMAKE_BUILD_TYPE)
if (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")
    target_compile_options(wasm-ql PRIVATE -D DEBUG)
endif (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wasm-ql PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics)
endif()
