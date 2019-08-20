from history-tools as builder

run apt-get update && apt-get install -y psmisc supervisor nginx

copy demo-gui /root/history-tools/demo-gui
copy demo-talk /root/history-tools/demo-talk
workdir /root/history-tools/demo-talk/src
run eosio-cpp talk.cpp

run \
    cleos wallet create --to-console | tail -n 1 | sed 's/"//g' >/password                      && \
    cleos wallet import --private-key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3       && \
    (  nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::state_history_plugin    \
       --disable-replay-opts --chain-state-history --trace-history                                 \
       -d /nodeos-data --config-dir /nodeos-config & )                                          && \
    sleep 2                                                                                     && \
    cleos create account eosio talk     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos set code talk talk.wasm                                                               && \
    cleos set abi talk talk.abi                                                                 && \
    cleos create account eosio adam     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio bill     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio bob      EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio jack     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio jane     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio jenn     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio jill     EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio joe      EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio sam      EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos create account eosio sue      EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV   && \
    cleos push action talk post '[100,0,adam,"First Post"]' -p adam                             && \
    cleos push action talk post    '[101,100,bill,"First Reply"]' -p bill                       && \
    cleos push action talk post       '[102,101,bob,"Nested"]' -p bob                           && \
    cleos push action talk post       '[103,101,jack,"Another"]' -p jack                        && \
    cleos push action talk post    '[110,100,jane,"Another Reply"]' -p jane                     && \
    cleos push action talk post '[200,0,jenn,"Second Post"]' -p jenn                            && \
    sleep 2                                                                                     && \
    killall nodeos                                                                              && \
    tail --pid=`pidof nodeos` -f /dev/null                                                      && \
    rm -rf /nodeos-data/snapshots /nodeos-data/state /nodeos-data/state-history                 && \
    rm -rf /nodeos-data/blocks/reversible                                                       && \
    ls -la /nodeos-data                                                                         && \
    du -h /nodeos-data

run chmod 755 /root
run mkdir -p /var/log/supervisor
copy demo-talk/supervisord.conf /etc/supervisor/conf.d/supervisord.conf
copy demo-talk/nginx-site.conf /etc/nginx/sites-available/default

workdir /root/history-tools/demo-talk
run npm i -g yarn
run yarn && yarn build

expose 80/tcp
cmd ["/usr/bin/supervisord"]
