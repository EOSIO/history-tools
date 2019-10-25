cmake_minimum_required(VERSION 3.10)

project(wasms)

set(EOSIO_WASM_OLD_BEHAVIOR "off")
find_package(eosio.cdt)

set(ROOT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(ROOT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/..)

function(add_server TARGET SRC)
    add_executable(${TARGET} ${SRC} ${ROOT_SOURCE_DIR}/libraries/eosiolib/wasmql/eosio/temp_placeholders.cpp)
    target_link_libraries(${TARGET} -fquery-server --eosio-imports=${ROOT_SOURCE_DIR}/libraries/eosiolib/wasmql/server.imports)
    target_compile_options(${TARGET} PUBLIC "-Os")
    target_include_directories(${TARGET}
        PUBLIC
        ${ROOT_SOURCE_DIR}/libraries/eosiolib/wasmql 
        ${ROOT_SOURCE_DIR}/external/abieos/external/date/include 
        )
    set_target_properties(${TARGET}
       PROPERTIES
       RUNTIME_OUTPUT_DIRECTORY ${ROOT_BINARY_DIR})
endfunction()

function(add_client TARGET SRC)
    add_executable(${TARGET} ${SRC} ${ROOT_SOURCE_DIR}/libraries/eosiolib/wasmql/eosio/temp_placeholders.cpp)
    target_link_libraries(${TARGET} -fquery-client --eosio-imports=${ROOT_SOURCE_DIR}/libraries/eosiolib/wasmql/client.imports)
    target_compile_options(${TARGET} PUBLIC "-Os")
    target_include_directories(${TARGET}
        PUBLIC
        ${ROOT_SOURCE_DIR}/libraries/eosiolib/wasmql 
        ${ROOT_SOURCE_DIR}/external/abieos/external/date/include 
        )
    set_target_properties(${TARGET}
       PROPERTIES
       RUNTIME_OUTPUT_DIRECTORY ${ROOT_BINARY_DIR})
endfunction()

function(add_other TARGET SRC)
    add_executable(${TARGET} ${SRC})
    target_link_libraries(${TARGET} -ftester)
    target_compile_options(${TARGET} PUBLIC "-Os")
    target_include_directories(${TARGET}
        PUBLIC
        ${ROOT_SOURCE_DIR}/libraries/eosiolib/wasmql 
        ${ROOT_SOURCE_DIR}/external/abieos/include
        ${ROOT_SOURCE_DIR}/external/abieos/src
        ${ROOT_SOURCE_DIR}/external/abieos/external/date/include 
        ${ROOT_SOURCE_DIR}/external/abieos/external/rapidjson/include 
        )
    set_target_properties(${TARGET}
       PROPERTIES
       RUNTIME_OUTPUT_DIRECTORY ${ROOT_BINARY_DIR})
endfunction()

# add_subdirectory(chain)
# add_subdirectory(legacy)
# add_subdirectory(token)

add_other(foo foo.cpp)
add_other(fill-system fill-system.cpp)
