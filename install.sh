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
echo 'Please run: source ~/.bashrc'
