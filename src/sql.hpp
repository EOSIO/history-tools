#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>
namespace SQL{


enum sql_type: uint8_t {
    Insert,
    Upsert,
    Delete,
    Create,
    None
};

struct sql{
    sql_type type = sql_type::None;
};


struct upsert: sql{

std::string table_name;
std::vector< std::tuple<std::string,std::string>> data;
std::vector< std::string> condition;
bool empty = true;


upsert(){
    type = sql_type::Upsert;
}

upsert(const std::tuple<std::string,std::string>& tp){
    upsert();
    data.push_back(tp);
    empty = false;
}

upsert(const std::string& field, const std::string& value){
    upsert();
    data.emplace_back(field,value);
    empty = false;
}


upsert& into(const std::string& _table_name){
    table_name = _table_name;
    return *this;
}

upsert& on_conflict(std::vector<std::string>& cols){
    condition = cols;
    return *this;
}

upsert& operator()(const std::tuple<std::string,std::string>& tp){
    data.push_back(tp);
    return *this;
}

upsert& operator()(const std::string& field, const std::string& value){
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
    ss<< " ON CONFLICT " << "(";
    for(uint32_t i = 0; i < condition.size(); ++i){
        if(i!=0)ss << ",";
        ss << condition[i];
    }
    ss<< " ) ";
    ss<< "DO UPDATE ";
    ss<< "SET ";

    bool first_hit = true;
    for(uint32_t i = 0; i < data.size(); ++i){
        if(std::find(condition.begin(),condition.end(),std::get<0>(data[i])) != condition.end())continue;
        if(!first_hit)ss << ",";
        first_hit = false;
        ss << std::get<0>(data[i]) << " = " << std::get<1>(data[i]);
    }
    return ss.str();
}

std::vector<std::string> get_columns(){
    std::vector<std::string> cols;
    for(uint32_t i = 0;i<data.size();++i){
        cols.push_back(std::get<0>(data[i]));
    }
    return cols;
}

std::vector<std::string> get_value(){
    std::vector<std::string> cols;
    for(uint32_t i = 0;i<data.size();++i){
        cols.push_back(std::get<1>(data[i]));
    }
    return cols;
}

};




struct insert: sql{

std::string table_name;
std::vector< std::tuple<std::string,std::string>> data;
bool empty = true;


insert(){
    type = sql_type::Insert;
}

insert(const std::tuple<std::string,std::string>& tp){
    insert();
    data.push_back(tp);
    empty = false;
}

insert(const std::string& field, const std::string& value){
    insert();
    data.emplace_back(field,value);
    empty = false;
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

std::vector<std::string> get_columns(){
    std::vector<std::string> cols;
    for(uint32_t i = 0;i<data.size();++i){
        cols.push_back(std::get<0>(data[i]));
    }
    return cols;
}

std::vector<std::string> get_value(){
    std::vector<std::string> cols;
    for(uint32_t i = 0;i<data.size();++i){
        cols.push_back(std::get<1>(data[i]));
    }
    return cols;
}

};


struct del: sql{
    std::string table_name;
    std::string condition;

    del(){
        type = sql_type::Delete;
    }

    del& from(const std::string& _name){
        table_name = _name;
        return *this;
    }

    del&  where(const std::string& _condidtion){
        condition = _condidtion;
        return *this;
    }

    std::string str(){
        std::stringstream ss;
        ss << "DELETE FROM " << table_name;
        ss << " WHERE " << condition;
    return ss.str();
}

};



struct enum_type{

    std::string type_name;
    std::vector<std::string> sub_types;
    enum_type(const std::string& name):type_name(name){}

    enum_type& operator()(const std::string& t){
        sub_types.push_back(t);
        return *this;
    }

    std::string str(){
        std::stringstream ss;
        ss << "CREATE TYPE " << type_name << " AS enum(";
        for(uint32_t i = 0; i < sub_types.size(); ++i){
            if(i!=0)ss << ",";
            ss << sub_types[i];
        }
        ss << ")";
        return ss.str();
    }

};

struct create: sql{

    std::string table_name;
    std::vector<std::tuple<std::string,std::string>> fields;
    std::string prim_key = {};

    create(const std::string& name){
        type = sql_type::Create;
        table_name = name;
    }

    create& operator()(const std::string& field, const std::string& type){
        fields.emplace_back(field,type);
        return *this;
    }

    create& operator()(const std::tuple<std::string,std::string>& tp){
        fields.push_back(tp);
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
            if(prim_key.size() != 0){
            ss<< ", primary key(" << prim_key <<")"; 
            }
            ss << ")";
            return ss.str();
    }
};


}

