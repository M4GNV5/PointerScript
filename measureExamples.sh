#!/bin/bash

mkdir graphs

function measureExample
{
	echo Measuring $1
	valgrind --tool=callgrind bin/ptrs examples/$1.ptrs
	../gprof2dot.py -f callgrind callgrind.out.* | dot -Tpng -o graphs/$1.png
	rm callgrind.out.*
	echo ""
}

measureExample circle
measureExample pi
measureExample types
measureExample array
measureExample struct
measureExample bubblesort
#measureExample fork
#measureExample threads
