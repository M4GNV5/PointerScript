# PointerScript
Dynamically typed scripting language with pointers and native library access. PointerScript
feels like C but has awesome features like operator overloading, dynamic typing and
even though you have direct low level access your code is more safe thanks to boundary
checks. Additionally finding errors is less painful as you get a full backtrace when a
runtime error occurs or you receive e.g. a segmentation fault. If you love low level
programming [you can even write inline assembly](LanguageDoc.md#asmstatement) (on linux-x86_64 only)

##Installing
Requirements are [libffi](https://github.com/libffi/libffi) and build tools.
The following instructions are for debian based distros however apart from dependency
installation there shouldnt be any difference to other distros.
```bash
#this line might differ if you dont have a debian based distro
sudo apt-get install libffi-dev build-essential

git clone --recursive https://github.com/M4GNV5/PointerScript
cd PointerScript

make release #if you are not on linux-x86_64 try 'make portable'
sudo make install #optional (copies bin/ptrs to /usr/local/bin/ptrs)
```

##Language

###Standard Library
PointerScript has no standard-library. You can use all C libraries using the built-in ffi ([Import statement](LanguageDoc.md#importstatement)),
however there are a couple of useful libraries and bindings (networking, regexp, http, json, lists, maps etc.)
in [this repo](https://github.com/M4GNV5/PtrsStuff)

###Performance
Generally PointerScript runs faster than many other scripting languages due to the absence of a Garbage Collector and the usage of native libraries.
You can create call graphs for the code examples in this repository using:
```bash
sudo apt-get install valgrind graphviz python3
curl -O ../gprof2dot.py https://raw.githubusercontent.com/jrfonseca/gprof2dot/master/gprof2dot.py
./measureExamples.sh
```
Currently the main time eaters are lookahead (parser) for small scripts without expensive loops and ptrs_scope_get for scripts with long loops

###Testing
You can run tests for the interpreter by executing the `runTests.sh` script in the repository

###Documentation
Most of PointerScript is similar to Javascript and/or C. For a full Documentation see [LanguageDoc.md](LanguageDoc.md)

##Example code
```javascript
//first we import some native functions from libc
import printf, puts, qsort from "libc.so.6";



//create a new array with 4 elements
//as we have an initializer the size is optional
//we could also write var nums[] = ...
var nums[4] = [1337, 42, 666, 31.12];



//PointerScript is dynamically typed, you can get the type of a variable using typeof
//the result will be the type id. You can obtain type ids of types using type<...>

//'pointer' is a pointer to a variable
typeof nums == type<pointer>;

//note there is no 'number' type like in Javascript. We have both 'int' and 'float'
//int is a 64 bit singed integer
typeof nums[0] == type<int>;
//'float' is a IEEE 64 bit double precision floating pointer number
//when referencing C types in PointerScript use 'single' and 'double'
typeof nums[3] == type<float>;

//you can convert types using cast<...>
nums[3] = cast<int>nums[3]; //sets nums[3] to 31
//for a reinterpret cast use as<...> (e.g. as<native>nums)



//sort the array using the libc function 'qsort'
//	'nums' is the array created above
//	'sizeof nums' is the length of the array (aka 4)
//	'VARSIZE' is a constant that holds the size of a variable (should be 16 bytes)
//	'compar' is a pointerscript function that will be passed like a normal C function pointer
function compar(a, b)
{
	return *as<pointer>a - *as<pointer>b;
}
qsort(nums, sizeof nums, VARSIZE, compar);

//instead of defining a function we could also use a lambda
qsort(nums, sizeof nums, VARSIZE, (a, b) -> *as<pointer>a - *as<pointer>b);



//iterate over each entry in 'vals'
foreach(i, val in nums)
{
	//you can insert variables and expression into strings
	//for variables use $name
	//for expressions use ${} (e.g. ${typeof val})
	puts("nums[$i] = $val");
}



//the above foreach statement can also be written as
for(var i = 0; i < sizeof nums; i++)
{
	//when using printf manually make sure to use the correct format for the
	//type of the variable passed, in this case %d for integers
	printf("nums[%d] = %d\n", i, nums[i]);
}
```

###More example code
There are examples including the usage of Types, Structs, Arrays, Threading and many more in the [examples](examples/) directory of this repository. The most interresting ones are listed here:

- [pi](examples/pi.ptrs) and [circle](examples/circle.ptrs) Basic mathematic expressions and loops
- [types](examples/types.ptrs) Using typeof and type<...>
- [fork](examples/fork.ptrs) Using posix functions for creating child processes
- [array](examples/array.ptrs) and [bubblesort](examples/bubblesort.ptrs) Basic array usage
- [struct](examples/struct.ptrs) Basic struct usage
- [asm](examples/asm.ptrs) Basic inline assembly usage
- [threads](examples/threads.ptrs) Using libpthread (or generally native functions that take function pointer arguments) with Pointerscript
- [gtk](examples/gtk.ptrs) Using GTK for creating a window with a clickable button.
- [window](examples/window.ptrs) Using libSDL for creating X windows. (Example orginally by [@Webfreak001](https://github.com/WebFreak001))
