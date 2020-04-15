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

    void complete(){
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
    virtual std::vector<std::string> create() = 0;
    virtual ~table_builder(){}
};


// Define user defined literal "_quoted" operator.
std::string operator"" _quoted(const char* text, std::size_t len) {
    return "'" + std::string(text, len) + "'";
}

std::string quoted(const std::string& text){
    return "'" + text + "'";
}


struct transaction_trace_builder:table_builder{
    std::optional<abieos::checksum256> block_id;
    std::optional<abieos::checksum256> transaction_id;
    uint32_t transaction_ordinal = 0;

    std::vector<std::string> create() override final {
        
        auto query = SQL::create("transaction_trace");
        query("block_num",                  "bigint")
             ("transaction_ordinal",        "integer")
             ("failed_dtrx_trace",          "varchar(64)")
             ("id",                         "varchar(64)")
             ("status",                     "transaction_status_type")
             ("cpu_usage_us",               "bigint")
             ("net_usage_words",            "bigint")
             ("elapsed",                    "bigint")
             ("net_usage",                  "numeric")
             ("scheduled",                  "boolean")
             ("account_ram_delta_present",  "boolean")
             ("account_ram_delta_account",  "varchar(13)")
             ("account_ram_delta_delta",    "bigint")
             ("exception",                  "varchar")
             ("error_code",                 "numeric")
             ("partial_present",            "boolean")
             ("partial_expiration",         "timestamp")
             ("partial_ref_block_num",      "integer")
             ("partial_ref_block_prefix",   "bigint")
             ("partial_max_net_usage_words","bigint")
             ("partial_max_cpu_usage_ms",   "smallint")
             ("partial_delay_sec",          "bigint")
             ("partial_signatures",         "varchar")
             ("partial_context_free_data",  "bytea")
             .primary_key("block_num, transaction_ordinal");

        std::vector<std::string> ret{query.str()};
        return ret;
    }


    std::string handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) override final{
        if(!block_id.has_value() || block_id.value() != pos.block_id){
            block_id.reset();
            block_id.emplace(pos.block_id);
            transaction_ordinal = 0;
        }
        
        state_history::transaction_trace_v0 trace_v0 = std::get<state_history::transaction_trace_v0>(trace);
        state_history::action_trace_v0 atrace = std::get<state_history::action_trace_v0>(action_trace);
        
        if(transaction_id.has_value() && transaction_id.value() == trace_v0.id){
            return "";
        }

        transaction_id.reset();
        transaction_id.emplace(trace_v0.id);
        ++transaction_ordinal;

        auto query = SQL::insert("block_num",std::to_string(pos.block_num))
            ("transaction_ordinal", std::to_string(transaction_ordinal))
            ("failed_dtrx_trace",    [&]{   if(!trace_v0.failed_dtrx_trace.empty()){
                                                auto r = std::get<state_history::transaction_trace_v0>(trace_v0.failed_dtrx_trace.front().recurse);
                                                return quoted(std::string(r.id));
                                            }else{
                                                return quoted(std::string());
                                            }}())
            ("id",  quoted(std::string(trace_v0.id)))
            ("status",quoted(state_history::to_string(trace_v0.status)))
            ("cpu_usage_us", std::to_string(trace_v0.cpu_usage_us))
            ("net_usage_words", std::string(trace_v0.net_usage_words))
            ("elapsed", std::to_string(trace_v0.elapsed))
            ("net_usage", std::to_string(trace_v0.net_usage))
            ("scheduled", (trace_v0.scheduled?"true":"false"))
            .into("transaction_trace");
        std::cout << query.str() << std::endl;
        return query.str();
    }

};




//builder for action trace table.
struct action_trace_builder: table_builder{

    std::vector<std::string> create() override final {
        std::vector<std::string> queries;

        auto q = SQL::enum_type("transaction_status_type");
           q("executed"_quoted)
            ("soft_fail"_quoted)
            ("hard_fail"_quoted)
            ("delayed"_quoted)
            ("expired"_quoted);

        queries.push_back(q.str());



        auto query = SQL::create("action_trace");
        query("block_num",              "bigint")
             ("timestamp",              "timestamp")
             ("transaction_id",         "varchar(64)")
             ("transaction_status",     "transaction_status_type")
             ("actor",                  "varchar(13)")
             ("permission",             "varchar(13)")
             ("action_oridinal",        "bigint")
             ("creator_action_oridnal", "bigint")
             ("receipt_present",        "boolean")
             ("receipt_receiver",       "varchar(13)")
             ("receipt_act_digesst",    "varchar(64)")
             ("receipt_global_sequence","numeric")
             ("receipt_recv_sequence",  "numeric")
             ("receipt_code_sequence",  "bigint")
             ("receipt_abi_sequence",   "bigint")
             ("act_account",            "varchar(13)")
             ("act_name",               "varchar(13)")
             ("act_data",               "varchar(13)")
             ("context_free",           "boolean")
             ("elapsed",                "bigint")
             ("error_code",             "numeric")
             ("action_ordinal",         "varchar")
             .primary_key("block_num, transaction_id, action_ordinal");
        
        queries.push_back(query.str());
        return queries;
    }



    std::string handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) override final{
        state_history::transaction_trace_v0 trace_v0 = std::get<state_history::transaction_trace_v0>(trace);
        state_history::action_trace_v0 atrace = std::get<state_history::action_trace_v0>(action_trace);
        
        auto query = SQL::insert("block_num",std::to_string(pos.block_num))
             ("timestamp",quoted(std::string(sig_block.timestamp)))
             ("transaction_id",quoted(std::string(trace_v0.id)))
             ("transaction_status",quoted(state_history::to_string(trace_v0.status)))
             ("action_ordinal",quoted(std::to_string(atrace.action_ordinal.value)))
             .into("action_trace");

        if(atrace.act.authorization.size()){
            query("actor",quoted(std::string(atrace.act.authorization[0].actor)))
                 ("permission",quoted(std::string(atrace.act.authorization[0].permission)));
        }
        
        std::cout << query.str() << std::endl;
        return query.str();
    }

};




struct block_info_builder: table_builder{
    uint32_t last_block = 0;

    std::vector<std::string> create() override final {
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

        std::vector<std::string> ret{query.str()};
        return ret;
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

        p.complete();
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


        table_builders.push_back(std::make_unique<action_trace_builder>());
        table_builders.push_back(std::make_unique<block_info_builder>());
        table_builders.push_back(std::make_unique<transaction_trace_builder>());
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
                auto queries = builder->create();
                for(auto& query: queries){
                    std::cout << query << std::endl;
                    p(query);
                }
            }
        }

        p.complete();
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