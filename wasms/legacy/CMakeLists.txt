add_server(legacy-server legacy-server.cpp legacy-server.hpp fpconv.c fpconv.h powers.h)

target_include_directories(legacy-server
    PUBLIC
    ${ROOT_SOURCE_DIR}/external/abieos/src
    ${ROOT_SOURCE_DIR}/external/abieos/external/rapidjson/include
    )

target_compile_options(legacy-server PUBLIC -DRAPIDJSON_64BIT=1 -DRAPIDJSON_48BITPOINTER_OPTIMIZATION=1)
