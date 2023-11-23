#!/bin/bash

set -e

make clean
make

rm -rf callgrind.out.* graphs/
mkdir -p graphs

function measureExampleWithO
{
	echo "Measuring $1 with -O$2"
	valgrind --tool=callgrind bin/ptrs "-O$2" "examples/$1.ptrs" > /dev/null 2>&1
	gprof2dot -f callgrind callgrind.out.* > "graphs/$1O$2.dot"
	dot -Tpng "graphs/$1O$2.dot" -o "graphs/$1O$2.png"
	rm callgrind.out.*
}

function measureExample
{
	measureExampleWithO $1 0
	measureExampleWithO $1 1
	measureExampleWithO $1 2
	measureExampleWithO $1 3
}

measureExample circle
measureExample pi
measureExample array
measureExample struct
measureExample bubblesort
measureExample fork
measureExample threads
