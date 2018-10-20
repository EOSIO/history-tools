# copyright defined in abieos/LICENSE.txt

cmake_minimum_required (VERSION 3.11)
project(abieos VERSION 0.1 LANGUAGES CXX)

FIND_PACKAGE(Boost 1.58 REQUIRED COMPONENTS date_time system iostreams)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(fill-postgress src/fill-postgress.cpp)
target_include_directories(fill-postgress PUBLIC external/abieos/src external/abieos/external/rapidjson/include PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(fill-postgress Boost::date_time Boost::system Boost::iostreams)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(fill-postgress PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics)
endif()
