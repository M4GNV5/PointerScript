# PointerScript

Dynamically typed scripting language with pointers and native library access. PointerScript
feels like C but has awesome features like operator overloading, dynamic typing and
even though you have direct low level access your code is more safe thanks to boundary
checks. Additionally finding errors is less painful as you get a full backtrace when a
runtime error occurs or you receive e.g. a segmentation fault.

### You can try the language online on the [playground](https://pointerscript.org/play/)

## Language

### Standard Library
PointerScript has no standard-library. You can use all C libraries using the built-in ffi ([Import statement](LanguageDoc.md#importstatement)).
There are a couple of useful libraries and bindings (sockets, regexp, http, json, lists, maps etc.)
in [this repository](https://github.com/M4GNV5/PtrsStuff)

### Testing
You can run tests for the interpreter by executing the `runTests.sh` script in the repository

### Documentation
Most of PointerScript is similar to Javascript and/or C. For a full Documentation see [LanguageDoc.md](LanguageDoc.md)

### Introduction
The following is quite a bit of unknown code, we'll go through it (and some other things) below.
Remember you can run and modify this code in your browser on the [playground](https://pointerscript.org/play/)
```javascript
//import the C functions puts and qsort
//using the import statement you can import any function from the C standard library
//using import foo, bar from "file.so" you can import functions from any C library
//using import foo, bar from "otherScript.ptrs" allows you to put your code into multiple files
import puts, qsort;

//this defines an array of 4 variables, here they are all initialized with int's,
//but you can actually but anything in there (floats, strings, functions, etc.)
//defining arrays using curly brackets like var foo{128}; creates byte arrays instead
var nums[4] = [1337, 666, 3112, 42];

//as we can call any C function here we call qsort to sort the array we just defined
//the last argument to qsort is a function pointer, here we use a lambda expression
qsort(nums, sizeof nums, sizeof var, (a, b) -> *as<pointer>a - *as<pointer>b);

//foreach allows us to easily iterate over arrays. Of course you could also use
//for(var i = 0; i < sizeof nums; i++) { /*...*/ } instead
foreach(i, val in nums)
{
	//string literals can be turned into string format expressions by putting $variableName
	//inside. Alternatively you could just use printf("nums[%d] = %d", i, val); but then
	//you need to be sure i and val are actually of type int
	puts("nums[$i] = $val");
}
```

### More example code
There are examples including the usage of Types, Structs, Arrays, Threading and many more in
the [examples](examples/) directory of this repository. The most interresting ones are listed here:

- [pi](examples/pi.ptrs) and [circle](examples/circle.ptrs) Basic mathematic expressions and loops
- [types](examples/types.ptrs) Using typeof and type<...>
- [fork](examples/fork.ptrs) Using posix functions for creating child processes
- [array](examples/array.ptrs) and [bubblesort](examples/bubblesort.ptrs) Basic array usage
- [struct](examples/struct.ptrs) Basic struct usage
- [asm](examples/asm.ptrs) Basic inline assembly usage
- [threads](examples/threads.ptrs) Using libpthread (or generally native functions that take function pointer arguments) with Pointerscript
- [gtk](examples/gtk.ptrs) Using GTK for creating a window with a clickable button.
- [window](examples/window.ptrs) Using libSDL for creating X windows. (Example orginally by [@Webfreak001](https://github.com/WebFreak001))

## Installing
Pointerscript uses [libjit](https://www.gnu.org/software/libjit/) which has to
be built first and is included in the repository as a submodule.
```bash
#this line might differ if you dont have a debian based distro
sudo apt-get install build-essential autoconf automake libtool

#Clone the repository
git clone --recursive https://github.com/M4GNV5/PointerScriptJIT

#Build libjit
cd PointerScript/libjit
./bootstrap
./configure
make -j4 #the j is optional but speeds up compilation (replace 4 by the number of your cores)

cd ..
make

#done! PointerScript is at ./bin/ptrs
bin/ptrs --help
```

There is also syntax highlighting for atom in the [language-atom](https://github.com/M4GNV5/language-pointerscript) repository.
Use the following commands to install:
```bash
git clone https://github.com/M4GNV5/language-pointerscript
cd language-pointerscript
apm link
```

## License
[EUPL v1.1](LICENSE.txt)

> Copyright (C) 2017 Jakob Löw (jakob@löw.com)
>
> Licensed under the EUPL, Version 1.1 or - as soon they will be approved by the European
> Commission - subsequent versions of the EUPL (the "Licence"); You may not use this work
> except in compliance with the Licence.
>
> You may obtain a copy of the Licence at:
> http://ec.europa.eu/idabc/eupl.html
>
> Unless required by applicable law or agreed to in writing, software distributed under
> the Licence is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF
> ANY KIND, either express or implied. See the Licence for the specific language
> governing permissions and limitations under the Licence.
