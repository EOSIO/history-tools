cd .cicd/helpers/
pip install configparser
pip install requests
python2.7 get_eos.py
echo $PWD
ls -lash
apt-get update
apt install -y $PWD/eosio.deb
nodeos --version