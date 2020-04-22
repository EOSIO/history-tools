// copyright defined in LICENSE.txt
#include "postgres_plugin.hpp"
#include "parser_plugin.hpp"
#include "state_history_plugin.hpp"
#include <fc/log/logger.hpp>
#include "postgres_config_utils.hpp"
#include <abieos.hpp>


namespace bpo       = boost::program_options;


// Define user defined literal "_quoted" operator.
std::string operator"" _quoted(const char* text, std::size_t len) {
    return "'" + std::string(text, len) + "'";
}

/**
 * quote function
 * controled by g__quoted_enable globlal varible
 * when we insert with insert, we need quote on string
 * wehn we use tablewriter we don't need quote
 */
bool g__quoted_enable = true;
std::string quoted(const std::string& text){
    if(g__quoted_enable)return "'" + text + "'";
    return text;
}


/**
 * type builder
 * create customized types before create other tables.
 * need to be push to builder vector first 
 */
struct type_builder:table_builder{

    type_builder(){
        name = "type builder";
    }

    std::vector<std::string> create() override final{
        std::vector<std::string> queries;

        auto q = SQL::enum_type("transaction_status_type");
           q("executed"_quoted)
            ("soft_fail"_quoted)
            ("hard_fail"_quoted)
            ("delayed"_quoted)
            ("expired"_quoted);

        queries.push_back(q.str());
        return queries;
    }

    std::vector<std::string> drop() override final{

        std::vector<std::string> queries;
        queries.push_back("DROP TYPE IF EXISTS transaction_status_type");
        return queries;

    }
};


struct transaction_trace_builder:table_builder{
    std::optional<abieos::checksum256> block_id;
    std::optional<abieos::checksum256> transaction_id;
    uint32_t transaction_ordinal = 0;

    transaction_trace_builder(){
        name = "transaction_trace";
    }

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

    std::vector<std::string> drop() override final {
        std::vector<std::string> queries;
        queries.push_back("DROP TABLE IF EXISTS transaction_trace");
        return queries;
    }

    std::vector<std::string> truncate(const state_history::block_position& pos) override final {
        std::vector<std::string> queries;
        queries.push_back( SQL::del().from("transaction_trace").where("block_num >= " + std::to_string(pos.block_num)).str());
        return queries;
    }


    SQL::insert handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) override final{
        if(!block_id.has_value() || block_id.value() != pos.block_id){
            block_id.reset();
            block_id.emplace(pos.block_id);
            transaction_ordinal = 0;
        }
        
        state_history::transaction_trace_v0 trace_v0 = std::get<state_history::transaction_trace_v0>(trace);
        state_history::action_trace_v0 atrace = std::get<state_history::action_trace_v0>(action_trace);
        
        if(transaction_id.has_value() && transaction_id.value() == trace_v0.id){
            return SQL::insert();
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
        return query;
    }

};




//builder for action trace table.
struct action_trace_builder: table_builder{

    action_trace_builder(){
        name = "action_trace";
    }

    std::vector<std::string> create() override final {
        std::vector<std::string> queries;

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

    std::vector<std::string> drop() override final {
        std::vector<std::string> queries;
        queries.push_back("DROP TABLE IF EXISTS action_trace");
        return queries;
    }

    std::vector<std::string> truncate(const state_history::block_position& pos) override final {
        std::vector<std::string> queries;
        queries.push_back( SQL::del().from("action_trace").where("block_num >= " + std::to_string(pos.block_num)).str());
        return queries;
    }

    SQL::insert handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) override final{
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
        
        return query;
    }

};


struct block_info_builder: table_builder{
    uint32_t last_block = 0;

    block_info_builder(){
        name = "block_info";
    }

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

    std::vector<std::string> drop() override final {
        std::vector<std::string> queries;
        queries.push_back("DROP TABLE IF EXISTS block_info");
        return queries;
    }

    std::vector<std::string> truncate(const state_history::block_position& pos) override final {
        std::vector<std::string> queries;
        queries.push_back( SQL::del().from("block_info").where("block_num >= " + std::to_string(pos.block_num)).str());
        return queries;
    }

    SQL::insert handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) override final{
        if(pos.block_num == last_block)return SQL::insert(); //one block only do once. 
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

        return query;
    }

};



struct abi_data_handler:table_builder{

    const ABI::action_def abi;


    abi_data_handler(const std::string& _name, ABI::action_def& _abi):abi(_abi){name = _name;}

    SQL::insert handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) override final{
        state_history::transaction_trace_v0 trace_v0 = std::get<state_history::transaction_trace_v0>(trace);
        state_history::action_trace_v0 atrace = std::get<state_history::action_trace_v0>(action_trace);

        //if this action is not our target, return.
        if (atrace.act.name != abieos::name(abi.name.c_str()) || atrace.act.account != abieos::name(abi.contract.c_str()))return SQL::insert();

        auto query = SQL::insert("block_num",std::to_string(pos.block_num))
            ("timestamp",quoted(std::string(sig_block.timestamp)))
            ("transaction_id",quoted(std::string(trace_v0.id)))
            ("action_ordinal",quoted(std::to_string(atrace.action_ordinal.value)))
            .into(name);

        abieos::input_buffer buffer = atrace.act.data;
        std::string error;
        for(auto& field: abi.fields){
            if(field.type == "name"){
                uint64_t data = 0;
                if(!abieos::read_raw(buffer,error,data)){
                    elog("error when parsing name");
                    return SQL::insert();
                }
                query(field.name, quoted(abieos::name_to_string(data)));
            }
            else if(field.type == "asset"){
                uint64_t amount,symbol;
                if(!abieos::read_raw(buffer,error,amount)){
                    elog("error when parsing amount");
                    return SQL::insert();
                }
                query(field.name + "_amount",std::to_string(amount));
                if(!abieos::read_raw(buffer,error,symbol)){
                    elog("error when parsing symbol");
                    return SQL::insert();
                }
                query(field.name + "_symbol",quoted(abieos::symbol_code_to_string(symbol >> 8)));
            }else{
                std::string dest;
                if(!abieos::read_string(buffer,error,dest)){
                    elog("error when parsing dest");
                    return SQL::insert();
                }
                query(field.name, quoted(std::string(dest)));
            }
        }
        return query;
    }

    std::vector<std::string> truncate(const state_history::block_position& pos) override final {
        std::vector<std::string> queries;
        queries.push_back( SQL::del().from(name).where("block_num >= " + std::to_string(pos.block_num)).str());
        return queries;
    }

    std::vector<std::string> create() override final {
        auto query = SQL::create(name);
        query("block_num",              "bigint")
             ("timestamp",              "timestamp")
             ("transaction_id",         "varchar(64)")
             ("action_ordinal",         "bigint");

        for(auto& field: abi.fields){
            if(field.type == "name"){
                query(field.name,"varchar(64)");
            }
            else if(field.type == "asset"){
                query(field.name + "_amount","bigint");
                query(field.name + "_symbol","varchar");
            }else{
                query(field.name, "varchar");
            }
        }

        query.primary_key("block_num, transaction_id, action_ordinal");

        std::vector<std::string> ret{query.str()};
        return ret;
    }


    std::vector<std::string> drop() override final {
        std::vector<std::string> queries;
        queries.push_back("DROP TABLE IF EXISTS " + name);
        return queries;
    }
};





struct postgres_plugin_impl: std::enable_shared_from_this<postgres_plugin_impl> {
    std::optional<bsg::scoped_connection>     applied_action_connection;
    std::optional<bsg::scoped_connection>     block_finish_connection;
    std::optional<bsg::scoped_connection>     fork_connection;

    std::vector<std::unique_ptr<table_builder>> table_builders;    
    std::vector<std::unique_ptr<abi_data_handler>> abi_handlers;
    //datas
    postgres_plugin& m_plugin;

    //database 
    std::string db_string;
    bool create_table = false;
    std::optional<pqxx::connection> conn;
    std::optional<pg::pipe> m_pipe;

    bool m_use_tablewriter = false; //bulk

    postgres_plugin_impl(postgres_plugin& plugin):m_plugin(plugin){}

    void handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace){
        
        if(!m_pipe.has_value())m_pipe.emplace(conn.value());

        for(auto& tb: table_builders){
            try{
                auto query = tb->handle(pos,sig_block,trace,action_trace);
                if(query.empty)continue;
                if(m_use_tablewriter){
                    auto& twriter = pg::table_writer_manager::instance().get_writer(query.table_name,query.get_columns());
                    twriter.write_row(query.get_value());
                }else{
                    m_pipe.value()(query.str());
                }
            }catch(...){
                elog("exception when handle update on table ${table}.",("table",tb->get_name()));
            }
        }


        for(auto& tb: abi_handlers){
            try{
                auto query = tb->handle(pos,sig_block,trace,action_trace);
                if(query.empty)continue;
                if(m_use_tablewriter){
                    auto& twriter = pg::table_writer_manager::instance().get_writer(query.table_name,query.get_columns());
                    twriter.write_row(query.get_value());
                }else{
                    m_pipe.value()(query.str());
                }
            }catch(...){
                elog("exception when handle update on abi defined table ${table}.",("table",tb->get_name()));
            }
        }

    }


    void handle_fork(const state_history::block_position& pos){
        if(!m_pipe.has_value())m_pipe.emplace(conn.value());

        for(auto& up_builder: table_builders){
            auto queries = up_builder->truncate(pos);
            for(auto& q: queries){
                m_pipe.value()(q);
            }
        }

        for(auto& up_builder: abi_handlers){
            auto queries = up_builder->truncate(pos);
            for(auto& q: queries){
                m_pipe.value()(q);
            }
        }
        ilog("fork happened block( ${bnum} )",("bnum",pos.block_num));
    }

    void handle(const state_history::block_position& pos, const state_history::block_position& lib_pos){
        if(m_pipe.has_value()){
            m_pipe.value().complete();
            m_pipe.reset();
        }

        /*
        use tablewrite to accelerate table building 
        when current receive block number is smaller than current lib number.
        */
        if(!m_use_tablewriter && pos.block_num < lib_pos.block_num){
            m_use_tablewriter = true;
            g__quoted_enable = false;
            //init a table writer manager
            pg::table_writer_manager::instance(db_string);
        }

        if(m_use_tablewriter && pos.block_num >= lib_pos.block_num){

            //closs all table writers, switch back to normal insert.
            pg::table_writer_manager::instance().close_writers();
            m_use_tablewriter = false;
            g__quoted_enable = true;
        }

        if(m_use_tablewriter){
            ilog("complete block ${num}, current lib ${lib}. table writer enable",("num",pos.block_num)("lib",lib_pos.block_num));
        }else{
            ilog("complete block ${num}, current lib ${lib}. normal mode.",("num",pos.block_num)("lib",lib_pos.block_num));
        }
    }

    uint32_t get_last_block_num(){
        assert(conn.has_value());
        pqxx::work w(conn.value());
        pqxx::row row = w.exec1("select block_num from block_info order by block_num desc limit 1");
        uint32_t block_num = row[0].as<uint32_t>();
        ilog("latest block number exist in database is ${n}",("n",block_num));
        return block_num;
    }


    void init(const appbase::variables_map& options){

        db_string = options.count("dbstring") ? options["dbstring"].as<std::string>() : std::string();
        create_table = options.count("postgres-create");


        if( options.count("action-abi") ) {
         const std::vector<std::string> key_value_pairs = options["action-abi"].as<std::vector<std::string>>();
         for (const auto& entry : key_value_pairs) {
            try {
               auto kv = parse_kv_pairs(entry);
               auto name = kv.first;
               auto abi = abi_def_from_file(kv.second, appbase::app().data_dir());
               abi_handlers.emplace_back(std::make_unique<abi_data_handler>(name, abi));
               ilog("add abi defined table ${tname}",("tname",name));
            } catch (...) {
               elog("Malformed action-abi provider: \"${val}\"", ("val", entry));
               throw;
            }
         }
      }



        applied_action_connection.emplace(
            appbase::app().find_plugin<parser_plugin>()->applied_action.connect(
                [&](const state_history::block_position& pos, const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace){
                    handle(pos,sig_block,trace,action_trace);
                }
            )
        );

        block_finish_connection.emplace(
            appbase::app().find_plugin<parser_plugin>()->block_finish.connect(
                [&](const state_history::block_position& pos, const state_history::block_position& lib_pos){
                    handle(pos, lib_pos);
                }
            )
        );

        fork_connection.emplace(
            appbase::app().find_plugin<parser_plugin>()->signal_fork.connect(
                [&](const state_history::block_position& pos){
                    handle_fork(pos);
                }
            )
        );


        table_builders.push_back(std::make_unique<type_builder>()); //type need to create first. 
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

    
        /**
         * create table when option set.
         * will first try to drop exist table 
         * then every thing will start to build up from scratch
         */
        if(create_table){

            //drop table schema in a revese direction, because type builder always the first. 
            pg::pipe p(conn.value());
            for(auto it = table_builders.rbegin();table_builders.rend() != it;it++){
                auto qs = (*it)->drop();
                for(auto& q:qs){
                    p(q);
                }
            }

            for(auto it = abi_handlers.rbegin();abi_handlers.rend() != it;it++){
                auto qs = (*it)->drop();
                for(auto& q:qs){
                    p(q);
                }
            }

            for(auto& builder: table_builders){
                auto queries = builder->create();
                for(auto& query: queries){
                    p(query);
                }
            }

            for(auto& builder: abi_handlers){
                auto queries = builder->create();
                for(auto& query: queries){
                    p(query);
                }
            }
            p.complete();
        }else{
            auto block_num = get_last_block_num();
            appbase::app().find_plugin<state_history_plugin>()->set_initial_block_num(block_num+1);
        }

    }


    void stop(){
        //when app stop, disconnect main database connection.
        if(conn.has_value()){
            conn.value().disconnect();
        }
    }
};




static appbase::abstract_plugin& _postgres_plugin = appbase::app().register_plugin<postgres_plugin>();

postgres_plugin::postgres_plugin():my(std::make_shared<postgres_plugin_impl>(*this)){
}

postgres_plugin::~postgres_plugin(){}


void postgres_plugin::set_program_options(appbase::options_description& cli, appbase::options_description& cfg) {
    auto op = cli.add_options();
    op("postgres-create", "create tables.");
    op("dbstring", bpo::value<std::string>(), "dbstring of postgresql");


    op("action-abi", bpo::value<std::vector<std::string>>()->composing(),
    "ABIs used when decoding action trace.\n"
    "There must be at least one ABI specified OR the flag action-no-abis must be used.\n"
    "ABIs are specified as \"Key=Value\" pairs in the form <account-name>=<abi-def>\n"
    "Where <abi-def> can be:\n"
    "   an absolute path to a file containing a valid JSON-encoded ABI\n"
    "   a relative path from `data-dir` to a file containing a valid JSON-encoded ABI\n"
    );
}


void postgres_plugin::plugin_initialize(const appbase::variables_map& options) {
    my->init(options);
    my->start();
}
void postgres_plugin::plugin_startup() {
}
void postgres_plugin::plugin_shutdown() {
    my->stop();
}