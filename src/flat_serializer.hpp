#pragma once

#include <string>
#include <vector>
#include "abieos.hpp"
#include <variant>
#include <exception>
//using namespace std::__1;

using db_type = std::string;


struct abi_holder{
    const abieos::abi_def* abi;
    const std::map<std::string, abieos::abi_type>* abi_types;

    static abi_holder& instance(){
        static abi_holder ah;
        return ah;
    }

    void init(const abieos::abi_def& _abi, const std::map<std::string, abieos::abi_type>& _abi_types){
        abi = &_abi;
        abi_types = &_abi_types;
    }

    const abieos::abi_type& get_type(const std::string& name){
        auto it = abi_types->find(name);
        if(it == abi_types->end()){
            throw std::runtime_error("abi type not found");
        }
        return it->second;
    }

    std::vector<std::string> get_keys(const std::string& name){
        for(auto t: abi->tables){
            if(t.name == abieos::name(name.c_str())){
                return t.key_names;
            }
        }
        return std::vector<std::string>();
    }
private:
    abi_holder(){};
};


struct serilization_exception: public std::exception{
    const char* waht() const throw (){
        return "flat serilizer exception.";
    }
};

#define SER_ASSERT(x) if(!(x))throw serilization_exception()

struct flat_serializer{
    std::vector< std::string> types;
    std::vector< std::string> names;

    flat_serializer(const std::vector<std::string>& _names, const std::vector<std::string>& _types):
    types(_types),
    names(_names)
    {}

    std::vector<std::tuple<std::string,db_type>> serialize(abieos::input_buffer& buffer){
        std::string error;
        std::vector<std::tuple<std::string,db_type>> result;

        for(uint32_t i = 0;i<types.size(); i++){
            auto& t = types[i];
            auto& n = names[i];
            if(t == "string"){
                std::string dest;
                SER_ASSERT(abieos::read_string(buffer,error,dest));
                result.emplace_back(n,dest);
            }
            else if(t == "name"){
                uint64_t data = 0;
                SER_ASSERT(abieos::read_raw(buffer,error,data));
                result.emplace_back(n,abieos::name_to_string(data));
            }
            else if(t == "asset"){
                uint64_t amount,symbol;
                SER_ASSERT(abieos::read_raw(buffer,error,amount));
                result.emplace_back(n+"_amount",std::to_string(amount));
                SER_ASSERT(abieos::read_raw(buffer,error,symbol));
                result.emplace_back(n+"_symbol",abieos::symbol_code_to_string(symbol >> 8));
                
            }
            else if(t == "bool"){
                bool data;
                SER_ASSERT(abieos::read_raw(buffer,error,data));
                result.emplace_back(n,std::to_string(data));
            }
            else if(t == "uint64"){
                uint64_t data;
                SER_ASSERT(abieos::read_raw(buffer,error,data));
                result.emplace_back(n,std::to_string(data));
            }else{
                return result;
            }
        }
        return result;
    }

};
