// copyright defined in LICENSE.txt

#include "fill_plugin.hpp"

#include <boost/algorithm/string.hpp>
#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;

static abstract_plugin& _fill_plugin = app().register_plugin<fill_plugin>();

fill_plugin::fill_plugin() {}
fill_plugin::~fill_plugin() {}

void fill_plugin::set_program_options(options_description& cli, options_description& cfg) {
   auto op   = cfg.add_options();
   auto clop = cli.add_options();
   op("fill-connect-to,f", bpo::value<std::string>()->default_value("127.0.0.1:8080"),
      "State-history endpoint to connect to (nodeos)");
   op("fill-trim,t", "Trim history before irreversible");
   clop("fill-skip-to,k", bpo::value<uint32_t>(), "Skip blocks before [arg]");
   clop("fill-stop,x", bpo::value<uint32_t>(), "Stop before block [arg]");
   // todo: remove? implement in rdb?
   // clop("fill-trx", bpo::value<std::vector<std::string>>(), "Filter transactions 'include:status:receiver:act_account:act_name'");
}

void fill_plugin::plugin_initialize(const variables_map& options) {}
void fill_plugin::plugin_startup() {}
void fill_plugin::plugin_shutdown() {}

/*
std::vector<state_history::trx_filter> fill_plugin::get_trx_filters(const variables_map& options) {
    try {
        std::vector<state_history::trx_filter> result;
        if (!options.count("fill-trx"))
            result.push_back({true});
        else {
            auto v = options["fill-trx"].as<std::vector<std::string>>();
            for (auto& s : v) {
                boost::erase_all(s, " ");
                std::vector<std::string> split;
                boost::split(split, s, [](char c) { return c == ':'; });

                state_history::trx_filter filt;
                if (split.size() > 0 && split[0] == "+")
                    filt.include = true;
                else if (split.size() > 0 && split[0] == "-")
                    filt.include = false;
                else
                    throw std::runtime_error("include must be '+' or '-'");

                if (split.size() > 1 && !split[1].empty())
                    filt.status = state_history::get_transaction_status(split[1]);
                if (split.size() > 2 && !split[2].empty())
                    filt.receiver = abieos::name{split[2].c_str()};
                if (split.size() > 3 && !split[3].empty())
                    filt.act_account = abieos::name{split[3].c_str()};
                if (split.size() > 4 && !split[4].empty())
                    filt.act_name = abieos::name{split[4].c_str()};

                result.push_back(filt);
            }
        }
        return result;
    } catch (std::exception& e) {
        throw std::runtime_error("--fill-trx: "s + e.what());
    }
}
*/
