#!/bin/bash

sudo apt-get install build-essential linux-headers-$(uname -r) git
sudo apt-get install libnuma-dev -y
sudo apt-get update -y
sudo apt-get install libnuma-dev -y
git submodule sync
git submodule update --init
echo export ONVM_HOME=$(pwd) >> ~/.bashrc
cd dpdk
echo export RTE_SDK=$(pwd) >> ~/.bashrc
echo export RTE_TARGET=x86_64-native-linuxapp-gcc  >> ~/.bashrc
echo export ONVM_NUM_HUGEPAGES=1024 >> ~/.bashrc
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"
cd -

git submodule init && git submodule update
sudo apt-get install libpcap-dev -y
sudo apt-get install libreadline-dev -y
cd ..
curl -R -O http://www.lua.org/ftp/lua-5.3.5.tar.gz
tar zxf lua-5.3.5.tar.gz
cd lua-5.3.5
make linux test
cd -
cd openNetVM
cd tools/Pktgen/pktgen-dpdk/
make

echo 'Please run: source ~/.bashrc'
echo 'and: ./scripts/install.sh'
echo 'To bind ports: ./dpdk/usertools/dpdk-setup.sh'
