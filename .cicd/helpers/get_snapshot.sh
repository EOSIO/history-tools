#get xml with info about snapshots
curl -L https://snapshot.testnet.eos.io > snapshots.xml

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
curl -L https://snapshot.testnet.eos.io/$LATEST_SNAPSHOT | tar -xzf -

mv snapshots/`ls snapshots` snapshot.bin
rm -rf snapshots.xml snapshots