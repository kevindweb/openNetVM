#!/bin/bash

# rebooting node will bring down the interface, make sure it's up
print_header "Configuring mtcp interface"
cd ~/mtcp
python3 ~/mtcp-bind.py
dpdk0_ip_addr="11.0.0.1"
sudo ifconfig dpdk0 $dpdk0_ip_addr netmask 255.255.255.0 up

print_header "Running ONVM Manager"
cd ~/repository/onvm
./go.sh 0,1,2,3 3 0xF0 -a 0x7f000000000 -s stdout &>~/onvm_stats &
mgr_pid=$?
if [ $mgr_pid -ne 0 ] 
then
    echo "ERROR: Starting manager failed"
    exit 1
fi

# wait for the manager to come online
sleep 15
print_header "Manager is live"

# go to mtcp directory
cd ~/mtcp/apps/example

print_header "Running EP Server"
sudo ./epserver -p ~/www -f ./epserver.conf -N 1 &>~/server_stats &
epserver_pid=$?
if [ $epserver_pid -ne 0 ] 
then
    echo "ERROR: Starting EP Server failed"
    return 1
fi

# wait for server to come online
sleep 10
print_header "EP Server is live"

print_header "Running EP Wget"
sudo ./epwget $dpdk0_ip_addr/perf.md 10000000 -N 1 -c 1024 -f ./epwget.conf &>~/client_stats &
epwget_pid=$?
if [ $epwget_pid -ne 0 ] 
then
    echo "ERROR: Starting EP Wget failed"
    return 1
fi

# wait for statistics to run
print_header "Collecting EP Server Statistics"
sleep 20

print_header "Killing EP Wget"
sudo pkill -f epwget 
check_exit_code "ERROR: Killing EP client failed"

print_header "Killing EP Server"
sudo pkill -f epserver
check_exit_code "ERROR: Killing EP server failed"

print_header "Exiting Manager"
sudo pkill -f onvm_mgr
check_exit_code "ERROR: Killing manager failed"
