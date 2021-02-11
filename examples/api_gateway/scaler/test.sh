#!/bin/bash

i=1

# infinite while loop example
while :
do
    echo "Here $i"
    sleep 1
    i=$((i+1))
done