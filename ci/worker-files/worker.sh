#!/bin/bash

if [[ -z $1 ]]
then
    echo "ERROR: Missing first argument, path to config file!"
    exit 1
fi

if [[ ! -f $1 ]]
then
    echo "ERROR: Could not find config file at given path!"
    exit 1
fi

. $1 # source config file

if [[ -z $WORKER_MODE ]]
then
    echo "ERROR: Missing mode argument of config!"
    exit 1
fi

# source helper functions file
. helper-functions.sh

#sudo apt-get install -y build-essential linux-headers-$(uname -r) git
#sudo apt-get install -y libnuma1
#sudo apt-get install -y libnuma-dev
#sudo apt-get install -y python3

cd repository
log "Beginning Execution of Workload"

log "Installing Environment"
install_env
check_exit_code "ERROR: Installing environment failed"

log "Building ONVM"
#build_onvm
check_exit_code "ERROR: Building ONVM failed"
exit 1
for mode in $WORKER_MODE
do
    # run functionality for each mode
    case "$mode" in
    "0")
        . ~/speed-worker.sh
        ;;  
    "1")
        . ~/pktgen-worker.sh
        ;;  
    "2")
        . ~/mtcp-worker.sh
        ;;  
    "3")
        . ~/speed-worker.sh
        . ~/pktgen-worker.sh
        . ~/mtcp-worker.sh
        ;;  
    *)  
        echo "Worker mode $mode has not been implemented"
        ;;  
    esac
done

log "Performance Tests Completed Successfully"
