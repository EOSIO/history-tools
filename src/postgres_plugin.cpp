// copyright defined in LICENSE.txt
#include "postgres_plugin.hpp"
#include "parser_plugin.hpp"
#include "state_history_plugin.hpp"
#include <fc/log/logger.hpp>
#include "postgres_config_utils.hpp"
#include <abieos.hpp>
#include "flat_serializer.hpp"
#include <tuple>
#include <deque>

namespace bpo       = boost::program_options;


// Define user defined literal "_pg_quoted" operator.
std::string operator"" _pg_quoted(const char* text, std::size_t len) {
    return "'" + std::string(text, len) + "'";
}

/**
 * quote function
 * controled by g__pg_quoted_enable globlal varible
 * when we insert with insert, we need quote on string
 * wehn we use tablewriter we don't need quote
 */
bool g__pg_quoted_enable = true;
std::string pg_quoted(const std::string& text){
    if(g__pg_quoted_enable)return "'" + text + "'";
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
           q("executed"_pg_quoted)
            ("soft_fail"_pg_quoted)
            ("hard_fail"_pg_quoted)
            ("delayed"_pg_quoted)
            ("expired"_pg_quoted);

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
                                                return pg_quoted(std::string(r.id));
                                            }else{
                                                return pg_quoted(std::string());
                                            }}())
            ("id",  pg_quoted(std::string(trace_v0.id)))
            ("status",pg_quoted(state_history::to_string(trace_v0.status)))
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
             ("timestamp",pg_quoted(std::string(sig_block.timestamp)))
             ("transaction_id",pg_quoted(std::string(trace_v0.id)))
             ("transaction_status",pg_quoted(state_history::to_string(trace_v0.status)))
             ("action_ordinal",pg_quoted(std::to_string(atrace.action_ordinal.value)))
             .into("action_trace");

        if(atrace.act.authorization.size()){
            query("actor",pg_quoted(std::string(atrace.act.authorization[0].actor)))
                 ("permission",pg_quoted(std::string(atrace.act.authorization[0].permission)));
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
             ("block_id",pg_quoted(std::string(pos.block_id)))
             ("timestamp",pg_quoted(std::string(sig_block.timestamp)))
             ("producer",pg_quoted(std::string(sig_block.producer)))
             ("confirmed",std::to_string(sig_block.confirmed))
             ("previous",pg_quoted(std::string(sig_block.previous)))
             ("transaction_count",std::to_string(sig_block.transactions.size()+1))
             ("transaction_mroot",pg_quoted(std::string(sig_block.transaction_mroot)))
             ("action_mroot",pg_quoted(std::string(sig_block.action_mroot)))
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
            ("timestamp",pg_quoted(std::string(sig_block.timestamp)))
            ("transaction_id",pg_quoted(std::string(trace_v0.id)))
            ("action_ordinal",pg_quoted(std::to_string(atrace.action_ordinal.value)))
            .into(name);

        abieos::input_buffer buffer = atrace.act.data;
        std::string error;

        std::vector<std::string> names,types;
        for(auto& field: abi.fields){
            names.push_back(field.name);
            types.push_back(field.type);
        }

        flat_serializer ser(names, types);

        auto result = ser.serialize(buffer);

        for(auto& t: result){
            query(std::get<0>(t), pg_quoted(std::get<1>(t)) );
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
                query(field.name + "_amount","varchar");
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


struct table_delta_handler:table_builder{


    virtual std::vector<std::string> create(){return std::vector<std::string>();}
    virtual std::vector<std::string> drop(){return std::vector<std::string>();}
    virtual std::vector<std::string> truncate(const state_history::block_position& pos){return std::vector<std::string>();}
    bool already_created = false;
    bool enable_cache = true;  
    uint32_t block_num = 0;
    std::optional<std::vector<std::string>> m_primary_key;
    std::optional<std::vector<std::string>> m_column_names;

    enum update_type: uint8_t{
        insert = 0,
        modify,
        del
    };

    std::deque<std::tuple<uint32_t,std::vector<std::string>,std::vector<std::string>>> cache;
    std::vector<std::string> pre_queries;
    std::vector<std::string> creation_queries;
    

    void generate_pre_update_queries(const state_history::block_position& pos, const std::vector<std::string>& prim_key, const std::vector<std::string>& prim_key_value){
        if(already_created == false)return;
        std::stringstream ss;
        ss << "SELECT * FROM " << name << " WHERE ";
        for(uint32_t i = 0; i< prim_key.size(); i++){
            if(i != 0)ss<< " AND ";
            ss << prim_key[i] << "=" << my_quoted(prim_key_value[i]);
        }
        pre_queries.push_back(ss.str());
    }

    std::vector<std::string> get_pre_queries(){
        std::vector<std::string> tmp(std::move(pre_queries));
        pre_queries.clear();
        return tmp;
    }

    void fill_cache(std::vector<std::string> row){
        std::map<std::string, std::string> name_to_value;
        for(int i = 0;i<row.size();i++){
            name_to_value.insert(std::make_pair(m_column_names.value()[i],row[i]));
        }
        std::vector<std::string> pk_v;
        for(auto& k: m_primary_key.value())pk_v.push_back(name_to_value[k]);
        cache.emplace_back(block_num,pk_v,row);
    }

    /**
     * row table back to status before pos
     */
    std::vector<std::string> roll_back(const state_history::block_position& pos){
        std::vector<std::string> result;
        while(!cache.empty() && std::get<0>(cache.back()) >= pos.block_num){

            std::map<std::string, std::string> name_to_value;
            auto& pk_values = std::get<1>(cache.back());
            auto& values = std::get<2>(cache.back());
            
            for(int i = 0;i<values.size();i++){
                name_to_value.insert(std::make_pair(m_column_names.value()[i],values[i]));
            }

            //this mean we don't have this row previously.
            if(values.empty()){

                std::stringstream ss;
                bool first_hit = true;
                for(auto& k: m_primary_key.value()){
                    if(first_hit){
                        first_hit = false;
                    }else{
                        ss << " and ";
                    }
                    ss << k << "=" << my_quoted(name_to_value[k]);
                }


                auto q = SQL::del();
                q.from(name).where(ss.str());
                result.push_back(q.str());
            }else{
                auto q = SQL::upsert();
                q.into(name).on_conflict(m_primary_key.value());
                for(auto& pair: name_to_value){
                    q(pair.first,my_quoted(pair.second));
                }
                result.push_back(q.str());
            }

            cache.pop_back();
        }
        return result;
    }

    void clean_cache(uint32_t lib_pos){
        while(!cache.empty() && std::get<0>(cache.front()) < lib_pos){
            // std::cout << "cache:" << std::get<0>(cache.front()) << " cache size:" << cache.size() << std::endl;
            // for(auto s: std::get<1>(cache.front()))std::cout << s;
            // std::cout << std::endl;
            // for(auto s: std::get<2>(cache.front()))std::cout << s;
            // std::cout << std::endl;
            cache.pop_front();
        }
    }

    
    std::string dynamic_create(std::vector<std::string> cols, std::vector<std::string> primary_key){

        if(!m_primary_key.has_value())m_primary_key.emplace(primary_key);
        if(!m_column_names.has_value())m_column_names.emplace(cols);

        auto query = SQL::create(name);
        for(auto c: cols){
            query(c, "varchar");
        }
        std::stringstream ss;
        for(int i = 0;i<primary_key.size();i++){
            if(i != 0)ss << ",";
            ss << primary_key[i];
        }
        query.primary_key(ss.str());

        return query.str();
    }

    //use this my quote function, because delta table alway use piple via main connection, so ignore global quote enable switch.
    std::string my_quoted(const std::string& text){
        return "'" + text + "'";
    }

    std::string remove_quote(const std::string& text){
        std::string tmp = text;
        if(tmp.size()<2)return tmp;
        if(tmp.at(0) == '"')tmp = tmp.substr(1,tmp.size()-1);
        if(tmp.at(tmp.size()-1) == '"')tmp = tmp.substr(0,tmp.size()-1);
        return tmp;
    }

    std::vector<std::string> handle_delta(const state_history::block_position& pos, const state_history::table_delta_v0& delta){
        block_num = pos.block_num;

        std::vector<std::string> result;

        //only handle delta update related with itself
        if(delta.name != name)return result;


        const abieos::abi_def& my_abi = *(abi_holder::instance().abi);
        const abieos::abi_type& my_type = abi_holder::instance().get_type(name);

        //primary key attached.
        auto keys = abi_holder::instance().get_keys(name);

        std::string error;
        std::string json_row;

        for(auto& row: delta.rows){
            abieos::input_buffer data = row.data;
            if(!abieos::bin_to_json(data,error,&my_type,json_row)){
                elog("error when serilizing data.");
            }
            fc::variant jdata = fc::json::from_string(json_row);
            auto& arr = jdata.get_array();
            auto& data_arr = arr[1].get_object();

            std::vector<std::string> cols;
            std::vector<std::string> values;
            std::map<std::string, std::string> name_to_value;
            for( auto itr = data_arr.begin(); itr != data_arr.end(); ++itr ){
                cols.push_back(itr->key());

                values.push_back(fc::json::to_string(itr->value()));
                name_to_value.insert(std::make_pair(cols.back(),values.back()));
            }

            if(!already_created){
                creation_queries.push_back("DROP TABLE IF EXISTS " + name);
                creation_queries.push_back( dynamic_create(cols,keys));
                already_created = true;
            }

            if(enable_cache && m_primary_key.has_value() && m_column_names.has_value()){
                std::vector<std::string> pk_v;
                for(auto& k: m_primary_key.value())pk_v.push_back(name_to_value[k]);
                generate_pre_update_queries(pos, m_primary_key.value(), pk_v);
            }


            if(row.present){

                auto in_query = SQL::upsert().into(name).on_conflict(keys);
                for(int i = 0;i<cols.size();i++){
                    in_query(cols[i],my_quoted(values[i]));
                }
                result.push_back(in_query.str());

            }else{

                std::stringstream condition;
                bool first_condition = true;
                for(int i = 0;i<cols.size();i++){
                    if(std::find(keys.begin(),keys.end(),cols[i]) != keys.end()){
                        if(!first_condition)condition << " and ";
                        condition << cols[i] << "=" << values[i];
                        first_condition = false;
                    }
                }

                auto de_query = SQL::del();
                de_query.from(name).where(condition.str());
                result.push_back(de_query.str());
            }
            
        }
        return result;
    }


    table_delta_handler(const std::string& _name){
        ilog("create delta table handler [${name}]",("name",_name));
        name = _name;
    }

};


struct postgres_plugin_impl: std::enable_shared_from_this<postgres_plugin_impl> {
    std::optional<bsg::scoped_connection>     applied_action_connection;
    std::optional<bsg::scoped_connection>     applied_delta_connection;
    std::optional<bsg::scoped_connection>     applied_abi_connection;
    std::optional<bsg::scoped_connection>     block_finish_connection;
    std::optional<bsg::scoped_connection>     fork_connection;

    std::vector<std::unique_ptr<table_builder>> table_builders;    
    std::vector<std::unique_ptr<abi_data_handler>> abi_handlers;
    std::vector<std::unique_ptr<table_delta_handler>> table_delta_handlers;
    //datas
    postgres_plugin& m_plugin;
    //status
    state_history::block_position m_current_lib;

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

        for(auto& delta_handler: table_delta_handlers){
            auto queries = delta_handler->roll_back(pos);
            for(auto& q: queries){
                m_pipe.value()(q);
            }
        }

        ilog("rollback database to block( ${bnum} )",("bnum",pos.block_num-1));
    }

    // ANCHOR :: this is the endofblock
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
            g__pg_quoted_enable = false;
            //init a table writer manager
            pg::table_writer_manager::instance(db_string);
        }

        if(m_use_tablewriter && pos.block_num >= lib_pos.block_num){

            //closs all table writers, switch back to normal insert.
            pg::table_writer_manager::instance().close_writers();
            m_use_tablewriter = false;
            g__pg_quoted_enable = true;
        }

        for(auto& handler: table_delta_handlers){
            handler->clean_cache(lib_pos.block_num);
        }

        m_current_lib = lib_pos;


        if(m_use_tablewriter){
            //only log per 1000 block
            if(pos.block_num%1000 == 0)ilog("complete block ${num}, current lib ${lib}. table writer enable",("num",pos.block_num)("lib",lib_pos.block_num));
        }else{
            ilog("complete block ${num}, current lib ${lib}. normal mode.",("num",pos.block_num)("lib",lib_pos.block_num));
        }
    }

    void handle(const state_history::block_position& pos, const state_history::table_delta_v0& delta){

        if(!m_pipe.has_value())m_pipe.emplace(conn.value());

        for(auto& handler: table_delta_handlers){

            if(!handler->creation_queries.empty()){
                for(auto q: handler->creation_queries){
                    m_pipe.value()(q);
                }
                handler->creation_queries.clear();
            }

            //create snapshot of current table
            auto pre_queries = handler->get_pre_queries();
            for(auto& preq: pre_queries){
                auto result = m_pipe.value().retrieve(preq);
                if (result.empty()) {
                    handler->fill_cache(std::vector<std::string>());
                } else {
                    auto row = result.front();
                    std::vector<std::string> myrow;
                    for(auto x: row){
                        myrow.push_back(x.as<std::string>());
                    }
                    handler->fill_cache(myrow);
                }
            }


            auto queries = handler->handle_delta(pos,delta);
            for(auto& q: queries){
                if(q.size() == 0)continue;
                m_pipe.value()(q);
            }
        }

        
    }


    void handle(const abieos::abi_def& abi, const std::map<std::string, abieos::abi_type>& abi_types){
        abi_holder::instance().init(abi,abi_types);
        ilog("abi_holder inited.");
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

      if(options.count("system-table")){
          const std::vector<std::string> table_names = options["system-table"].as<std::vector<std::string>>();
          for(auto& tname: table_names){
            table_delta_handlers.emplace_back(std::make_unique<table_delta_handler>(tname));
            if(!create_table)table_delta_handlers.back()->already_created = true; //if we don't want to create table, directly set already created tag to true
            ilog("monitor system table ${tname}",("tname", tname));
          }
      }


        applied_action_connection.emplace(
            appbase::app().find_plugin<parser_plugin>()->applied_action.connect(
                [&](const state_history::block_position& pos, const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace){
                    handle(pos,sig_block,trace,action_trace);
                }
            )
        );

        applied_delta_connection.emplace(
            appbase::app().find_plugin<parser_plugin>()->applied_delta.connect(
                [&](const state_history::block_position& pos, const state_history::table_delta_v0& delta){
                    handle(pos,delta);
                }
            )
        );

        applied_abi_connection.emplace(
            appbase::app().find_plugin<state_history_plugin>()->applied_abi.connect(
                [&](const abieos::abi_def& abi, const std::map<std::string, abieos::abi_type>& abi_types){
                    handle(abi,abi_types);
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


        //roll every table back to status on LIB
        //just treat this as a fork.
        handle_fork(m_current_lib);

        if(m_pipe.has_value())m_pipe.value().complete();

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

    op("system-table", bpo::value<std::vector<std::string>>()->composing(),"System state tables.");
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