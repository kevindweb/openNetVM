#!/bin/bash

i=1

# infinite while loop example
while :
do
    echo "Here $i"
    sleep 1
    i=$((i+1))
    if [ "$i" -gt 20 ]; then
        # intentionally fail to see if we're brought back up
        exit 1
    fi
done