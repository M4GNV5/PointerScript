#!/bin/bash

function runExample
{
	echo Running example $1
	bin/ptrs examples/$1.ptrs

	local status=$?
	if [ $status -ne 0 ]; then
		local red='\033[0;31m'
		local nocolor='\033[0m'
		printf "${red}ERROR${nocolor} running example $1\n"
		exit $?
	fi
	echo ""
}

runExample circle
runExample pi
runExample types
runExample array
runExample struct
runExample fork
runExample asm
runExample bubblesort
runExample threads
