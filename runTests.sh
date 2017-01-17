#!/bin/bash

red='\033[0;31m'
green='\033[0;32m'
yellow='\033[0;33m'
nocolor='\033[0m'

function runTest
{
	printf "${yellow}TRYING${nocolor} test $1\n"
	bin/ptrs tests/$1.ptrs

	local status=$?
	if [ $status -ne 0 ]; then
		printf "\n${red}ERROR${nocolor} running test $1\n"
	else
		printf "\e[1A${green}SUCCESS${nocolor} running test $1\n"
	fi
}

runTest runtime/interop
runTest runtime/pointer
runTest runtime/types
runTest runtime/trycatch
runTest runtime/strformat
runTest runtime/struct
runTest runtime/alignment
