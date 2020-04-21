#pragma once
#include "sql.hpp"
#include <pqxx/pqxx>
#include <pqxx/connection>
#include <pqxx/tablewriter>


namespace pg{

struct work{
    pqxx::work w;
    work(pqxx::connection& con):w(con){}

    work& operator()(std::string& query){
        try
        {
            w.exec(query);
        }
        catch(const pqxx::usage_error& e)
        {
            std::cerr << e.what() << '\n';
            std::cerr << query << std::endl;
        }
        
        return *this;
    }

    ~work(){
        w.commit();
    }

};


struct pipe{
    pqxx::work w;
    pqxx::pipeline p;
    std::vector<int> ids;
    pipe(pqxx::connection& con):w(con),p(w){}

    pipe& operator()(std::string query){
        if(query.empty())return *this;
        ids.push_back(p.insert(query));
        return *this;
    }

    pqxx::result retrieve(std::string& query){
        return p.retrieve(p.insert(query));
    }

    void complete(){

        p.complete();

        for(auto i: ids){
            if(!p.is_finished(i)){
                throw std::runtime_error("problem!!!! pipeline is not complete.");
            }
        }

        w.commit();
    }
};

struct writer{
    pqxx::connection con;
    pqxx::work w;
    pqxx::tablewriter tw;

    writer(const std::string& db_str ,const std::string& name, const std::vector<std::string> cols):
    con(db_str),w(con),tw(w,name,cols.begin(),cols.end()){}

    void write_row(std::vector<std::string> row){
        tw << row;
    }

    void complete(){
        tw.complete();
        w.commit();
    }

};


class table_writer_manager{

    std::map< std::string, std::shared_ptr<writer>> table_writers; 
    std::string db_str;

    table_writer_manager(const std::string& _db_str):db_str(_db_str){}
public:
    static table_writer_manager& instance(std::optional<std::string> _db_str = std::nullopt){
        static table_writer_manager tbw_mgr(_db_str.value());
        return tbw_mgr;
    }

    
    writer& get_writer(const std::string& table_name, const std::vector<std::string>& cols){
        std::stringstream key;
        key << table_name << "::";
        for(auto& c: cols){
            key << c;
        };

        auto it = table_writers.find(key.str());
        if(it == table_writers.end()){
            it = table_writers.insert(std::make_pair(key.str(),std::make_shared<writer>(db_str,table_name,cols))).first;
        }

        return *(it->second);
    }

    void close_writers(){
        for(auto& pair: table_writers){
            pair.second->complete();
        }

        table_writers.clear();
    }

};

};