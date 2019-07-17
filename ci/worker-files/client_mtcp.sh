#!/bin/bash

echo "Running client side mTCP"

sudo /home/kdeems/mtcp/apps/example/epwget $1/perf.md 100000 -N 1 -c 1024 -f /home/kdeems/mtcp/apps/example/epwget.conf &>~/client_stats &

# sleep to wait for stats to build
sleep $2

echo "Killing client"
sudo pkill epwget
