cd .cicd/helpers/
pip install configparser
pip install requests
python2.7 get_eos.py
echo $PWD
cp -a $PWD/builds/current/programs/. /root/history-tools/programs
echo /root/history-tools/programs/nodeos/nodeos --version