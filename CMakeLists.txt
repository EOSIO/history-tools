# copyright defined in abieos/LICENSE.txt

cmake_minimum_required (VERSION 3.11)
project(abieos VERSION 0.1 LANGUAGES CXX)

FIND_PACKAGE(Boost 1.58 REQUIRED COMPONENTS date_time system iostreams program_options)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(fill-postgresql src/fill-postgresql.cpp)
target_include_directories(fill-postgresql PUBLIC external/abieos/src external/abieos/external/rapidjson/include PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(fill-postgresql Boost::date_time Boost::system Boost::iostreams Boost::program_options pqxx pq)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(fill-postgresql PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics)
endif()
