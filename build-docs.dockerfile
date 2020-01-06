from eosio/history-tools:build-docs-base

# generate docs
workdir /root
run mkdir -p /root/history-tools/src
copy book.json /root/history-tools
copy doc /root/history-tools/doc
copy external /root/history-tools/external
copy generate-doc /root/history-tools
copy libraries /root/history-tools/libraries
copy src/HistoryTools.js /root/history-tools/src
workdir /root/history-tools
run bash generate-doc

# result is in /root/history-tools/_book
run ls -l /root/history-tools/_book
