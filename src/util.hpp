#pragma once

//#include "abieos_exception.hpp"
#include <eosio/stream.hpp>

#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <fstream>

inline std::string read_string(const char* filename) {
    try {
        std::fstream file(filename, std::ios_base::in | std::ios_base::binary);
        file.seekg(0, std::ios_base::end);
        auto len = file.tellg();
        file.seekg(0, std::ios_base::beg);
        std::string result(len, 0);
        file.read(result.data(), len);
        return result;
    } catch (const std::exception& e) {
        throw std::runtime_error("Error reading " + std::string(filename));
    }
}

inline std::vector<char> zlib_decompress(eosio::input_stream data) {
    std::vector<char>                   out;
    boost::iostreams::filtering_ostream decomp;
    decomp.push(boost::iostreams::zlib_decompressor());
    decomp.push(boost::iostreams::back_inserter(out));
    boost::iostreams::write(decomp, data.pos, data.end - data.pos);
    boost::iostreams::close(decomp);
    return out;
}
