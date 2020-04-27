#pragma once
#include "sql.hpp"
#include <pqxx/pqxx>
#include <pqxx/connection>
#include <pqxx/tablewriter>
#include <optional>


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
    std::vector<std::string> qqs;
    pipe(pqxx::connection& con):w(con),p(w){}

    pipe& operator()(std::string query){
        if(query.empty())return *this;
        ids.push_back(p.insert(query));
        qqs.push_back(query);
        return *this;
    }

    pqxx::result retrieve(std::string& query){
        return p.retrieve(p.insert(query));
    }

    void complete(){

        p.complete();

        for(auto i: ids){
            if(!p.is_finished(i)){
                for(auto& qq: qqs){
                    std::cout << qq << std::endl;
                }
                throw std::runtime_error("problem!!!! pipeline is not complete.");
            }
        }

        w.commit();
    }
};

struct writer{
    pqxx::connection con;
    std::optional<pqxx::work> w;
    std::optional<pqxx::tablewriter> tw;
    std::string m_name;
    std::vector<std::string> m_cols;

    //countrol refresh
    int32_t refresh_counter = 0;
    int32_t refresh_interval = -1;

    writer(const std::string& db_str ,const std::string& name, const std::vector<std::string> cols, int32_t ritv = -1):
    con(db_str),
    m_name(name),
    m_cols(cols),
    refresh_interval(ritv){
        w.emplace(con);
        tw.emplace(w.value(),name,cols.begin(),cols.end());
    }

    void write_row(std::vector<std::string> row){
        if(!tw.has_value() || !w.has_value())return;
        tw.value() << row;

        if(refresh_interval != -1){
            if(++refresh_counter == refresh_interval){
                refresh();
                refresh_counter = 0;
            }
        }
    }

    void complete(){
        if(!tw.has_value() || !w.has_value())return;
        tw.value().complete();
        w.value().commit();
    }

    //commit curret table writer and restart a new one.
    void refresh(){
        complete();
        tw.reset();
        w.reset();
        w.emplace(con);
        tw.emplace(w.value(),m_name,m_cols.begin(),m_cols.end());
    }

    ~writer(){
        con.disconnect();
    }

};


class table_writer_manager{

    std::map< std::string, std::shared_ptr<writer>> table_writers; 
    std::string db_str;
    int32_t default_refresh_interval = 2000;
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
            it = table_writers.insert(std::make_pair(key.str(),std::make_shared<writer>(db_str,table_name,cols,default_refresh_interval))).first;
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