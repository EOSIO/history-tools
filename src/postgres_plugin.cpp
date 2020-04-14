// copyright defined in LICENSE.txt
#include "postgres_plugin.hpp"
#include "parser_plugin.hpp"
#include <fc/log/logger.hpp>
#include "sql.hpp"
#include <pqxx/pqxx>
#include <pqxx/connection>
#include <pqxx/tablewriter>


namespace bpo       = boost::program_options;

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

    pipe(pqxx::connection& con):w(con),p(w){}

    pipe& operator()(std::string& query){
        p.insert(query);
        return *this;
    }

    pqxx::result retrieve(std::string& query){
        return p.retrieve(p.insert(query));
    }

    ~pipe(){
        p.complete();
        w.commit();
    }
};

struct writer{
    pqxx::work w;
    pqxx::tablewriter tw;

    writer(pqxx::connection& con ,const std::string& name, const std::vector<std::string> cols):w(con),tw(w,name,cols.begin(),cols.end()){}

    void operator<<(std::vector<std::string>& row){
        tw << row;
    }

    ~writer(){
        tw.complete();
        w.commit();
    }

};

};





struct table_builder{
    virtual std::string handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) = 0;
    virtual std::string create() = 0;
    virtual ~table_builder(){}
};


// Define user defined literal "_quoted" operator.
std::string operator"" _quoted(const char* text, std::size_t len) {
    return "\"" + std::string(text, len) + "\"";
}

std::string quoted(const std::string& text){
    return "'" + text + "'";
}


struct action_trace_builder: table_builder{

    std::string create() override final {
        auto query = SQL::create("action_trace");
        query("block_num",      "bigint")
             ("timestamp",      "timestamp")
             ("transaction_id", "bigint")
             .primary_key("transaction_id");
        
        return query.str();
    }



    std::string handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) override final{
        state_history::transaction_trace_v0 trace_v0 = std::get<state_history::transaction_trace_v0>(trace);
        state_history::action_trace_v0 atrace = std::get<state_history::action_trace_v0>(action_trace);
        
        auto query = SQL::insert("block_num",std::to_string(pos.block_num))
             ("timestamp",std::string(sig_block.timestamp))
             ("transaction_id",std::string(trace_v0.id))
             ("transaction_status",state_history::to_string(trace_v0.status)).into("transaction_trace");

        if(atrace.act.authorization.size()){
            query("actor",std::string(atrace.act.authorization[0].actor))
                 ("permission",std::string(atrace.act.authorization[0].permission));
        }
        
        std::cout << query.str() << std::endl;
        return query.str();
    }

};




struct block_info_builder: table_builder{
    uint32_t last_block = 0;

    std::string create() override final {
        auto query = SQL::create("block_info");
        query("block_num",              "bigint")
             ("block_id",               "varchar(64)")
             ("timestamp",              "timestamp")
             ("producer",               "varchar(13)")
             ("confirmed",              "integer")
             ("previous",               "varchar(64)")
             ("transaction_count",      "integer")
             ("transaction_mroot",      "varchar(64)")
             ("action_mroot",           "varchar(64)")
             ("schedule_version",       "bigint")
             ("new_producers_version",  "bigint")
             .primary_key("block_num");

        return query.str();
    }


    std::string handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) override final{
        if(pos.block_num == last_block)return ""; //one block only do once. 
        state_history::transaction_trace_v0 trace_v0 = std::get<state_history::transaction_trace_v0>(trace);
        state_history::action_trace_v0 atrace = std::get<state_history::action_trace_v0>(action_trace);
        
        auto query = SQL::insert("block_num",std::to_string(pos.block_num))
             ("block_id",quoted(std::string(pos.block_id)))
             ("timestamp",quoted(std::string(sig_block.timestamp)))
             ("producer",quoted(std::string(sig_block.producer)))
             ("confirmed",std::to_string(sig_block.confirmed))
             ("previous",quoted(std::string(sig_block.previous)))
             ("transaction_count",std::to_string(sig_block.transactions.size()+1))
             ("transaction_mroot",quoted(std::string(sig_block.transaction_mroot)))
             ("action_mroot",quoted(std::string(sig_block.action_mroot)))
             ("schedule_version",std::to_string(sig_block.schedule_version))
             .into("block_info")
             ;
        last_block = pos.block_num; //avoid multiple insert
        std::cout << query.str() << std::endl;
        return query.str();
    }

};








struct postgres_plugin_impl: std::enable_shared_from_this<postgres_plugin_impl> {
    std::optional<bsg::scoped_connection>     applied_action_connection;
    std::vector<std::unique_ptr<table_builder>> table_builders;    
    //datas
    postgres_plugin& m_plugin;

    //database 
    std::string db_string;
    bool create_table;
    std::optional<pqxx::connection> conn;

    postgres_plugin_impl(postgres_plugin& plugin):m_plugin(plugin),create_table(false){}

    void handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace){
        
        pg::pipe p(conn.value());

        for(auto& tb: table_builders){
            auto query = tb->handle(pos,sig_block,trace,action_trace);
            p(query);
        }
    }


    void init(const appbase::variables_map& options){

        db_string = options.count("dbstring") ? options["dbstring"].as<std::string>() : std::string();
        create_table = options.count("postgres-create");

        applied_action_connection.emplace(
            appbase::app().find_plugin<parser_plugin>()->applied_action.connect(
                [&](const state_history::block_position& pos, const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace){
                    handle(pos,sig_block,trace,action_trace);
                }
            )
        );


        // table_builders.push_back(std::make_unique<action_trace_builder>());
        table_builders.push_back(std::make_unique<block_info_builder>());
    }


    void start(){

        //create_connection;
        try
        {
            conn.emplace(db_string);         
        }
        catch(const pqxx::usage_error& e)
        {
            std::cerr << e.what() << '\n';
        }
        

        assert(conn.has_value());

        pg::pipe p(conn.value());

        if(create_table){
            for(auto& builder: table_builders){
                auto query = builder->create();
                std::cout << query << std::endl;
                p(query);
            }
        }
    }

};




static appbase::abstract_plugin& _postgres_plugin = appbase::app().register_plugin<postgres_plugin>();

postgres_plugin::postgres_plugin():my(std::make_shared<postgres_plugin_impl>(*this)){
}

postgres_plugin::~postgres_plugin(){}


void postgres_plugin::set_program_options(appbase::options_description& cli, appbase::options_description& cfg) {
    auto op = cli.add_options();
    op("postgres-create", "create tables");
    op("dbstring", bpo::value<std::string>(), "dbstring of postgresql");
}


void postgres_plugin::plugin_initialize(const appbase::variables_map& options) {
    my->init(options);
}
void postgres_plugin::plugin_startup() {
    my->start();
}
void postgres_plugin::plugin_shutdown() {
}