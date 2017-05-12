#!/bin/bash

function get_ppid {
	echo `cat /proc/$1/status | grep PPid | awk {'print $2'}`
}

function kill_grandparent {
	HELPER_PPID=`get_ppid $1`
	kill `get_ppid $HELPER_PPID`
}

kill_grandparent $1
