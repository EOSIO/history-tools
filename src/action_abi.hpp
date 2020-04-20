#pragma once
#include <string>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/container/flat.hpp>
#include <vector>
#include "util.hpp"

namespace ABI{



using namespace std;

using type_name = std::string;
using field_name = std::string;


struct field_def {
   field_def() = default;
   field_def(const field_name& name, const type_name& type)
   :name(name), type(type)
   {}

   field_name name;
   type_name  type;

   bool operator==(const field_def& other) const {
      return std::tie(name, type) == std::tie(other.name, other.type);
   }
};



struct action_def{

   action_def() = default;
   action_def(const type_name& name, const type_name& base, const type_name& contract, const vector<field_def>& fields)
   :name(name), base(base), fields(fields)
   {}

   type_name            name;
   type_name            base;
   type_name            contract;
   vector<field_def>    fields;

   bool operator==(const action_def& other) const {
      return std::tie(name, base, contract, fields) == std::tie(other.name, other.base, other.contract, other.fields);
   }

};


}

FC_REFLECT( ABI::field_def, (name)(type) )
FC_REFLECT( ABI::action_def, (name)(base)(contract)(fields) )


void from_variant( const fc::variant& var,  ABI::field_def& vo ){
    vo.name = var["name"].as<std::string>();
    vo.type = var["type"].as<std::string>();
}


void from_variant( const fc::variant& var,  ABI::action_def& vo ){
    vo.contract = var["contract"].as<std::string>();
    vo.base = var["base"].as<std::string>();
    vo.name = var["name"].as<std::string>();

    auto& vars = var["fields"].get_array();
    for(auto v: vars){
      vo.fields.push_back(ABI::field_def());
      from_variant(v,vo.fields.back());
    }

}

/**
* Given a path (absolute or relative) to a file that contains a JSON-encoded ABI, return the parsed ABI
*
* @param file_name - a path to the ABI
* @param data_dir - the base path for relative file_name values
* @return the ABI implied
* @throws json_parse_exception if the JSON is malformed
*/
ABI::action_def abi_def_from_file(const std::string& file_name, const fc::path& data_dir )
{
    fc::variant abi_variant;
    auto abi_path = fc::path(file_name);
    if (abi_path.is_relative()) {
        abi_path = data_dir / abi_path;
    }

    // EOS_ASSERT(fc::exists(abi_path) && !fc::is_directory(abi_path), chain::plugin_config_exception, "${path} does not exist or is not a file", ("path", abi_path.generic_string()));
    std::cout << abi_path.generic_string() << std::endl;
    try {
        abi_variant = fc::json::from_file(abi_path);
    } catch(fc::assert_exception& e){
        elog("Fail to parse JSON from file: ${file}", ("file", abi_path.generic_string()));
        throw e;
    }
    ABI::action_def result;
    from_variant(abi_variant,result);
    return result;
}