#pragma once
#include <string>
#include <vector>
#include <queue>
#include <tuple>
#include "state_history.hpp"
#include "sql.hpp"
#include <memory>
#include "abieos.hpp"

#include <variant>
using namespace std::__1;

using db_type = variant<std::string, uint32_t, uint64_t, int32_t, int64_t>;

struct flat_serializer{
    std::vector< std::string> types;

    flat_serializer(const std::vector<std::string>& _types):
    types(_types)
    {}

    std::vector<db_type> serialize(abieos::input_buffer& buffer){
        std::string error;
        std::vector<db_type> result;

        for(auto& t: types){
            if(t == "string"){
                std::string dest;
                assert(abieos::read_string(buffer,error,dest));
                result.emplace_back(dest);
            }
        }
    }

};




struct delta_table{

    std::string table_name;
    std::vector< std::string> columns;
    std::vector< std::string> column_typs;
    std::queue<std::tuple<uint32_t, state_history::row>> cache_rows;
    uint32_t last_commit;
    uint32_t last_update;


    delta_table(const std::string& name, std::vector<std::string>& cols, std::vector<std::string>& col_types):
    table_name(name),
    columns(cols),
    column_typs(col_types)
    {}


    std::vector<std::shared_ptr<SQL::sql>> update(const state_history::block_position& pos, const state_history::table_delta_v0& delta){
        if(delta.name != table_name)return std::vector<std::shared_ptr<SQL::sql>>{};

        std::vector<std::shared_ptr<SQL::sql>> result;
        for(auto& row: delta.rows){
            
            if(row.present){
                //flat abi serilization
            }else{

            }
        }

    }


};



struct delta_table_factory{




};