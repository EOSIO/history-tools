// copyright defined in LICENSE.txt
#include "state_history_connection.hpp"
#include "state_history_plugin.hpp"
#include "util.hpp"
#include "state_history.hpp"
#include <optional>
#include "abieos.hpp"



struct state_history_plugin_impl: state_history::connection_callbacks, std::enable_shared_from_this<state_history_plugin_impl>{

    std::optional<std::string> host;
    std::optional<std::string> port;
    abieos::abi_def     m_abi = {};
    std::map<std::string, abieos::abi_type>     m_abi_types = {};
    bool m_irrversible_only = false;
    bool first_connect = true;
    std::optional<uint32_t> trace_begin_block;
    std::optional<uint32_t> state_begin_block;
    state_history_plugin&   m_plugin;

    std::shared_ptr<state_history::connection>   connection;
    //define signals 
    std::optional<uint32_t> initial_block_num;
    boost::asio::deadline_timer             timer;

    state_history_plugin_impl(state_history_plugin& plugin):m_plugin(plugin),timer(appbase::app().get_io_service()){
    }

    void set(const std::string& host, const std::string& port){
        this->host.emplace(host);
        this->port.emplace(port);
    }


    void start(boost::asio::io_context& ioc){
        state_history::connection_config config = {host.value(),port.value()};
        connection = std::make_shared<state_history::connection>(ioc, config, shared_from_this());
        connection->connect();

    }


    void request_blocks(uint32_t start_block_num, bool irrversible_only = false) {
        state_history::get_blocks_request_v0 req;
        req.start_block_num        = start_block_num;
        req.end_block_num          = 0xffff'ffff;
        req.max_messages_in_flight = 0xffff'ffff;
        req.have_positions         = {};
        req.irreversible_only      = irrversible_only;
        req.fetch_block            = true;
        req.fetch_traces           = true;
        req.fetch_deltas           = true;
        connection->send(req);
    }

    //override. 
    void received_abi(std::string_view abi_sv) override{
        ilog("reaceive abi");
        if(first_connect){
            json_to_native(m_abi, abi_sv);
            abieos::check_abi_version(m_abi.version);
            m_abi_types = abieos::create_contract(m_abi).abi_types;

            m_plugin.applied_abi(m_abi,m_abi_types);        
            connection->send(state_history::get_status_request_v0{});
            first_connect = false;
        }

    }
    bool received(state_history::get_status_result_v0& status) override{
        m_plugin.applied_status(status);
        ilog("receive status: ${a} ${b} ${c} ${d} ${e} ${f}",
            ("a",status.head.block_num)
            ("b",status.last_irreversible.block_num)
            ("c",status.trace_begin_block)
            ("d",status.trace_end_block)
            ("e",status.chain_state_begin_block)
            ("f",status.chain_state_end_block)
        );
        trace_begin_block.emplace(status.trace_begin_block);
        state_begin_block.emplace(status.chain_state_begin_block);

        uint32_t request_start_block = trace_begin_block.value();
        if(initial_block_num.has_value()){
            if(initial_block_num.value()>request_start_block){
                request_start_block = initial_block_num.value();
            }
        }
        request_blocks(request_start_block,m_irrversible_only);
        return true;
    }


    virtual bool received(state_history::get_blocks_result_v0& result) override{ 
        m_plugin.applied_blocks(result);
        // ilog("receive block ${num} ${bid}",("num",result.this_block.value().block_num)("bid",std::string(result.this_block.value().block_id)));
        return true;
    }

    void schedule_retry() {
        timer.expires_from_now(boost::posix_time::seconds(1));
        timer.async_wait([this](auto&) {
            ilog("retry...");
            start(appbase::app().get_io_service());
        });
    }

    void closed(bool retry) override {
            connection.reset();
            schedule_retry();
    }


    void set_initial_block_num(uint32_t block_num){
        if(initial_block_num.has_value())return;
        initial_block_num.emplace(block_num);
    }

    void irrversible_only(){
        m_irrversible_only = true;
    }

    void shutdown(){
        timer.cancel();
    }

};

static appbase::abstract_plugin& _state_history_plugin = appbase::app().register_plugin<state_history_plugin>();

state_history_plugin::state_history_plugin(){
    my = std::make_shared<state_history_plugin_impl>(*this);
}
state_history_plugin::~state_history_plugin(){}


void state_history_plugin::set_program_options(appbase::options_description& cli, appbase::options_description& cfg) {
    auto op = cfg.add_options();
    op("state_history_host", appbase::bpo::value<std::string>()->default_value("127.0.0.1"), "State History Plugin server");
    op("state_history_port", appbase::bpo::value<std::string>()->default_value("8080"), "State History Plugin port");
    op("start_block_number", "manual start block number");
    op("irrversible-only","irrversible only mode.");
    ilog("state history plugin set option.");
}


void state_history_plugin::plugin_initialize(const appbase::variables_map& options) {
    std::string host = options["state_history_host"].as<std::string>();
    std::string port = options["state_history_port"].as<std::string>();
    my->set(host,port);
    
    if(options.count("start_block_number")){
        uint64_t number = options["start_block_number"].as<uint64_t>();
        my->set_initial_block_num(number);
    }

    if(options.count("irrversible-only")){
        my->irrversible_only();
    }

    ilog("state history plugin init.");
}
void state_history_plugin::plugin_startup() {
    my->start(appbase::app().get_io_service());
    ilog("state history plugin startup.");
}
void state_history_plugin::plugin_shutdown() {
    ilog("state history plugin shutdown.");
    my->shutdown();
}

void state_history_plugin::set_initial_block_num(uint32_t block_num){
    my->set_initial_block_num(block_num);
}