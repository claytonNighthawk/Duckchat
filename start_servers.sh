#!/bin/bash

#uncomment the topolgy you want. The simple two-server topology is uncommented here.

# Change the SERVER variable below to point your server executable.
SERVER=./server

SERVER_NAME=`echo $SERVER | sed 's#.*/\(.*\)#\1#g'`

# Generate a simple two-server topology
$SERVER localhost 4000 localhost 4001 &
$SERVER localhost 4001 localhost 4000 & 

# Generate a simple three-server line topology
# ./server localhost 4000 localhost 4001 &
# ./server localhost 4001 localhost 4000 localhost 4002 & 
# ./server localhost 4002 localhost 4001 & 

# Generate a simple three-server loop topology
# ./server localhost 4000 localhost 4001 localhost 4002 &
# ./server localhost 4001 localhost 4000 localhost 4002 &
# ./server localhost 4002 localhost 4001 localhost 4000 &

# Generate a capital-H shaped topology
# $SERVER localhost 4000 localhost 4001 &
# $SERVER localhost 4001 localhost 4000 localhost 4002 localhost 4003 &
# $SERVER localhost 4002 localhost 4001 & 
# $SERVER localhost 4003 localhost 4001 localhost 4005 &
# $SERVER localhost 4004 localhost 4005 &
# $SERVER localhost 4005 localhost 4004 localhost 4003 localhost 4006 &
# $SERVER localhost 4006 localhost 4005 &

# Generate a 3x3 grid topology
# $SERVER localhost 4500 localhost 4501 localhost 4503 &
# $SERVER localhost 4501 localhost 4500 localhost 4502 localhost 4504 &
# $SERVER localhost 4502 localhost 4501 localhost 4505 &
# $SERVER localhost 4503 localhost 4500 localhost 4504 localhost 4506 &
# $SERVER localhost 4504 localhost 4501 localhost 4503 localhost 4505 localhost 4507 &
# $SERVER localhost 4505 localhost 4502 localhost 4504 localhost 4508 &
# $SERVER localhost 4506 localhost 4503 localhost 4507 &
# $SERVER localhost 4507 localhost 4506 localhost 4504 localhost 4508 &
# $SERVER localhost 4508 localhost 4505 localhost 4507 &


echo "Press ENTER to quit"
read
pkill $SERVER_NAME