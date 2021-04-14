# copyright defined in LICENSE.txt

include(FetchContent)

cmake_minimum_required (VERSION 3.10)
project(history-tools VERSION 1.0.0 LANGUAGES CXX)

cmake_policy(SET CMP0077 NEW)
set(ENABLE_INSTALL OFF cache bool "")
set(ENABLE_TOOLS OFF cache bool "")
set(ENABLE_TESTS OFF cache bool "")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
set(Boost_USE_STATIC_LIBS ON)

add_definitions(-DBOOST_ASIO_DISABLE_STD_EXPERIMENTAL_STRING_VIEW)

find_package(PostgreSQL REQUIRED COMPONENTS Libraries)
find_package(Boost 1.70 REQUIRED COMPONENTS date_time filesystem chrono system iostreams program_options unit_test_framework)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
      message(FATAL_ERROR "GCC version must be at least 8.0.")
   endif()
   if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
      add_compile_options(-fdiagnostics-color=always)
   endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/fc/CMakeModules")

# add defaults for openssl
if(APPLE AND UNIX AND "${OPENSSL_ROOT_DIR}" STREQUAL "")
   set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@1.1")
endif()

set(SKIP_FC_TESTS ON)

FetchContent_Declare(
  fc
  GIT_REPOSITORY https://github.com/EOSIO/fc.git
  GIT_TAG 50d57cbc6b2d1af6fd4a0930a5c98f7add900078       
)
FetchContent_MakeAvailable(fc)

FetchContent_Declare(
  appbase
  GIT_REPOSITORY https://github.com/EOSIO/appbase.git
  GIT_TAG 144b2e239d6fd93a8336543bf9eda7c52ea8c77e       
)
FetchContent_MakeAvailable(appbase)

FetchContent_Declare(
  abieos
  GIT_REPOSITORY https://github.com/EOSIO/abieos.git
  GIT_TAG 47657a1c0ed907991bc76ac684027b374ddba497       
)
FetchContent_MakeAvailable(abieos)

FetchContent_Declare(
  libpqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG a6b1d60e74c1427c8ac2324b85cd4a0dc2068332       
)
set(SKIP_PQXX_SHARED ON)
set(SKIP_BUILD_TEST ON)
set(PQXX_LIBRARIES pqxx_static)

FetchContent_MakeAvailable(libpqxx)

# Build Types
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}
    CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel asan ubsan"
    FORCE)

# AddressSanitize
set(CMAKE_C_FLAGS_ASAN
    "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C compiler during AddressSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_ASAN
    "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C++ compiler during AddressSanitizer builds."
    FORCE)

# UndefinedBehaviour
set(CMAKE_C_FLAGS_UBSAN
    "-fsanitize=undefined"
    CACHE STRING "Flags used by the C compiler during UndefinedBehaviourSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_UBSAN
    "-fsanitize=undefined"
    CACHE STRING "Flags used by the C++ compiler during UndefinedBehaviourSanitizer builds."
    FORCE)

set(WITH_TESTS OFF)

add_compile_options(-Wno-error=shadow)


add_executable(fill-pg src/main.cpp src/fill_plugin.cpp src/pg_plugin.cpp src/fill_pg_plugin.cpp src/abieos_sql_converter.cpp)

target_include_directories(fill-pg
    PRIVATE
        ${Boost_INCLUDE_DIR}
)
target_link_libraries(fill-pg appbase fc abieos Boost::date_time Boost::filesystem Boost::chrono 
                      Boost::system Boost::iostreams Boost::program_options Boost::unit_test_framework 
                      "${PQXX_LIBRARIES}" -lpthread)

if(APPLE)
else()
    target_link_libraries(fill-pg -latomic)
endif()
target_compile_options(fill-pg PUBLIC -DAPP_NAME="fill-pg" "-DDEFAULT_PLUGINS=fill_pg_plugin;-DINCLUDE_FILL_PG_PLUGIN")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(fill-pg PRIVATE -D DEBUG)
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(fill-pg PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics -Wno-sign-compare -Wno-unused-variable -Wno-macro-redefined)
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(fill-pg PRIVATE -fdiagnostics-color=auto)
endif()

add_subdirectory( unittests )

