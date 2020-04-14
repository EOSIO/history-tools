#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <sstream>

namespace SQL{

struct insert{

std::string table_name;
std::vector< std::tuple<std::string,std::string>> data;



insert(const std::tuple<std::string,std::string>& tp){
    data.push_back(tp);
}

insert(const std::string& field, const std::string& value){
    data.emplace_back(field,value);
}


insert& into(const std::string& _table_name){
    table_name = _table_name;
    return *this;
}

insert& operator()(const std::tuple<std::string,std::string>& tp){
    data.push_back(tp);
    return *this;
}

insert& operator()(const std::string& field, const std::string& value){
    data.emplace_back(field,value);
    return *this;
}

std::string str(){
    std::stringstream ss;
    ss << "INSERT INTO " << table_name << " ( ";
    for(uint32_t i = 0; i < data.size(); ++i){
        if(i!=0)ss << ",";
        ss << std::get<0>(data[i]);
    }
    ss<< " ) VALUES ( ";
    for(uint32_t i = 0; i < data.size(); ++i){
        if(i!=0)ss << ",";
        ss << std::get<1>(data[i]);
    }
    ss<< " )";
    return ss.str();
}
};


struct create{
    std::string table_name;
    std::vector<std::tuple<std::string,std::string>> fields;
    std::string prim_key;

    create(const std::string& name){
        table_name = name;
    }

    create& operator()(const std::string& field, const std::string& type){
        fields.emplace_back(field,type);
        return *this;
    }

    void primary_key(const std::string& pk){
        prim_key = pk;
    }

    std::string str(){
            std::stringstream ss;
            ss << "CREATE TABLE " << table_name << " ( ";
            for(uint32_t i = 0; i < fields.size(); ++i){
                if(i!=0)ss << ",";
                ss << std::get<0>(fields[i]) << " " << std::get<1>(fields[i]);
            }
            ss<< ", primary key(" << prim_key <<") )";
            return ss.str();
    }
};


}

