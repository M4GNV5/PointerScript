#!/bin/bash

red='\033[0;31m'
green='\033[0;32m'
yellow='\033[0;33m'
nocolor='\033[0m'

hadError=0

function runTestWithArgs
{
	if [ "$2" != "" ]; then
		withArguments=" with arguments $2"
	else
		withArguments=""
	fi

	printf "${yellow}TRYING${nocolor} test $1$withArguments\n"
	bin/ptrs $2 tests/$1.ptrs

	#printf "${yellow}TRYING${nocolor} test $1 without flow analysis\n"
	#bin/ptrs --no-flow tests/$1.ptrs

	local status=$?
	if [ $status -ne 0 ]; then
		printf "\n${red}ERROR${nocolor} running test $1$withArguments\n"
		hadError=1
	else
		printf "\e[1A${green}SUCCESS${nocolor} running test $1$withArguments\n"
	fi
}

function runTest
{
	runTestWithArgs "$1"
	runTestWithArgs "$1" -O0
	runTestWithArgs "$1" -O1
	runTestWithArgs "$1" -O2
	runTestWithArgs "$1" --no-predictions
	runTestWithArgs "$1" "--no-predictions -O0"
	runTestWithArgs "$1" "--no-predictions -O1"
	runTestWithArgs "$1" "--no-predictions -O2" 
}

runTest runtime/interop "$1"
runTest runtime/pointer "$1"
runTest runtime/types "$1"
runTest runtime/conversion "$1"
runTest runtime/loops "$1"
runTest runtime/switch "$1"
#runTest runtime/trycatch TODO
runTest runtime/strformat "$1"
runTest runtime/struct "$1"
runTest runtime/map "$1"
runTest runtime/overload "$1"
runTest runtime/functions "$1"
runTest runtime/alignment "$1"
runTest runtime/operators "$1"

if [ $hadError -ne 0 ]; then
	exit 1
fi
