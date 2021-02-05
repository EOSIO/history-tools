#get xml with info about snapshots
wget -O snapshots.xml https://snapshot.testnet.eos.io

#parse xml to get latest snaspshot
read_dom () {
    local IFS=\>
    read -d \< ENTITY CONTENT
}

LATEST_SNAPSHOT=""

while read_dom; do
    if [[ $ENTITY = "Key" ]]; then
      if [[ $CONTENT =~ "snapshots" ]]; then
        LATEST_SNAPSHOT=$CONTENT
      fi
    fi
done < snapshots.xml

#get actual snapshot file and unzip
wget https://snapshot.testnet.eos.io/$LATEST_SNAPSHOT
tar -xzvf snapshots.tar.gz

