# PointerScript
Dynamically typed scripting language with pointers and native library access. PointerScript
feels like C but has awesome features like operator overloading, dynamic typing and
even though you have direct low level access your code is more safe thanks to boundary
checks. Additionally finding errors is less painful as you get a full backtrace when a
runtime error occurs or you receive e.g. a segmentation fault.

###You can try the language online on the [playground](https://pointerscript.org/play/)

##Installing
Requirements are [libffi](https://github.com/libffi/libffi) and build tools.
The following instructions are for debian based distros however apart from dependency
installation there shouldnt be any difference to other distros.
```bash
#this line might differ if you dont have a debian based distro
sudo apt-get install libffi-dev build-essential

git clone --recursive https://github.com/M4GNV5/PointerScript
cd PointerScript

make
sudo make install #optional (copies bin/ptrs to /usr/local/bin/ptrs)
```

There is also syntax highlighting for atom in the [language-atom](https://github.com/M4GNV5/language-pointerscript) repository.
Use the following commands to install:
```bash
git clone https://github.com/M4GNV5/language-pointerscript
cd language-pointerscript
apm link
```

##Language

###Standard Library
PointerScript has no standard-library. You can use all C libraries using the built-in ffi ([Import statement](LanguageDoc.md#importstatement)).
There are a couple of useful libraries and bindings (sockets, regexp, http, json, lists, maps etc.)
in [this repository](https://github.com/M4GNV5/PtrsStuff)



###Performance
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



##Introduction
The following is quite a bit of unknown code, we'll go through it (and some other things) below.
Remember you can run and modify this code in your browser on the [playground](https://pointerscript.org/play/)
```javascript
import puts, qsort;

var nums[4] = [1337, 666, 3112, 42];
qsort(nums, sizeof nums, sizeof var, (a, b) -> *as<pointer>a - *as<pointer>b);

foreach(i, val in nums)
{
	puts("nums[$i] = $val");
}
```

You can import any C function like `printf`, `qsort` etc.
```javascript
import printf, puts, qsort from "libc.so.6";
```

The `from` part of the import statement is optional, without it for the symbol will be searched in the default library search order (aka for libc and posix functions leaving out the from should work however when you want to use a library such as curl you need it)
```javascript
import printf, puts, qsort;
```

Defining arrays of variables is similar to C. Note that either length or initializer must be given, defining both will fill up remaining entries with the last initialized one or throw an error if the initializer is too long for the given length.
```javascript
var nums[4] = [1337, 42, 666, 31.12];
```

When defining byte arrays use `{}` instead of `[]`. Additionally byte arrays can be initialized by string expressions
```javascript
var msg{} = {'h', 'i', 0};
//or
var msg2{128} = "hello world!";
```

When you want to create arrays on the heap you can use `new array` with either square brackets for arrays of variables or curly brackets for byte arrays. Note: PointerScript has no garbage collector thus you have to free the memory of the array later using `delete`
```javascript
var nums2 = new array[8] ["hi", msg, nums, 3.14];
delete nums2;
```

PointerScript is dynamically typed, you can get the type of a variable using typeof. The result is a type id. You can obtain type ids using `type<...>`. e.g. `typeof nums` (from above) is `type<pointer>` and `typeof msg` is `type<native>`. The former one points to one or more variables the latter one to one or more ubytes.
```javascript
typeof nums == type<pointer>;
typeof msg == type<native>;
```

There is no 'number' type like in Javascript. We have both `int` and `float`.
`int` is a 64 bit singed integer while `float` is a IEEE 64 bit double precision floating point number. Note that this is different to most other languages. When you need 32bit single precision floats use `single`.
```javascript
typeof nums[0] == type<int>;
typeof nums[3] == type<float>;
```

you can convert types using `cast<...>``
```javascript
nums[3] = cast<int>nums[3]; //sets nums[3] to 31
```
for a reinterpret cast use `as<...>` (e.g. `as<native>nums`)

Now lets actually use a C function, in thise case `qsort` for sorting the array `nums` (see `man 3 qsort` for more information about `qsort`). We pass 4 arguments:
- `nums` is the array created above
- `sizeof nums` is the length of the array (aka 4)
- `VARSIZE` is a constant that holds the size of a variable (should be 16 bytes)
- `compar` is a pointerscript function that will be passed like a normal C function pointer

```javascript
function compar(a, b)
{
	return *as<pointer>a - *as<pointer>b;
}
qsort(nums, sizeof nums, VARSIZE, compar);
```

Instead of defining a function we could also use a lambda
```javascript
qsort(nums, sizeof nums, VARSIZE, (a, b) -> *as<pointer>a - *as<pointer>b);
```

Now we want to output the contents of `nums`. First we use fancy features like foreach and string insertions.
```javascript
foreach(i, val in nums)
{
	//you can insert variables and expression into strings
	//for variables use $name
	//for expressions use ${} (e.g. ${typeof val})
	puts("nums[$i] = $val");
}
```

But of course we can also use printf directly
```javascript
for(var i = 0; i < sizeof nums; i++)
{
	//when using printf make sure to use the correct format for the
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



###License
PointerScript - Scripting language with pointers and native library access

Copyright (C) 2017 Jakob Löw (jakob@löw.com)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

[You should have received a copy of the GNU General Public License
along with this program.](LICENSE.md)
