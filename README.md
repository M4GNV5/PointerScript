# PointerScript

Scripting language with pointers and native library access. PointerScript
feels like C but has awesome features like operator overloading, dynamic typing and
even though you have direct low level access your code is more safe thanks to boundary
checks. Additionally finding errors is less painful as you get a full backtrace when a
runtime error occurs or you receive e.g. a segmentation fault.

## Language

### Documentation
Most of PointerScript is similar to Javascript and/or C. For a full Documentation see [LanguageDoc.md](LanguageDoc.md)

### Standard Library
PointerScript has no standard-library. You can use all C libraries using the built-in ffi ([Import statement](LanguageDoc.md#importstatement)).
There are a couple of easy-to-use libraries (sockets, regexp, http, json, lists, maps etc.)
in [this repository](https://github.com/M4GNV5/PtrsStuff)

### Testing
You can run tests for the interpreter by executing the `runTests.sh` script in the repository.

### Introduction
The following is a small code sample, annotated with comments to explain what is happening.
```javascript
//using the import statement you can import any function from the C standard library
import puts, qsort;

//this defines an array of 4 signed 32-bit integers.
//Using var nums: var[4]; we could create an array of dynamically typed variables instead.
var nums: i32[4] = [1337, 666, 3112, 42];

//here we cann the standard C function qsort to sort the array
//the last argument to qsort is a function pointer, here we use a lambda expression
qsort(nums, sizeof nums, sizeof i32, (a: i32*, b: i32*) -> *a - *b);

//using the foreach loop on arrays is similar to a for loop from 0 to sizeof nums
foreach(i, val in nums)
{
	// the language as built-in string formatting
	puts("nums[$i] = $val");
}
```

## JIT Compilation
There are three main steps involved when running PointerScript code:
- parsing the source code (handeled by the built-in recusive descent parser)
- predicting types of expressions
- converting the code to GNU [libjit](https://www.gnu.org/software/libjit/) IR
- converting the IR to machine code (handled by [my fork of libjit](https://github.com/M4GNV5/libjit/tree/ptrs-graphalloc))

The following shows the process of these steps inspecting the lambda function from the above
code example.

### Predicting types
Using the `--dump-predictions` flag to view predictions made by PointerScript:
```
(a: i32*, b: i32*) -> *a - *b
                      ││ │ │└─> meta: i32[1] ast: identifier
                      ││ │ └──> meta: int ast: prefix_dereference
                      ││ └────> type: int ast: op_sub
                      │└──────> meta: i32[1] ast: identifier
                      └───────> meta: int ast: prefix_dereference
```

### libjit IR
Using the `--dump-jit` flag to dump libjit IR of the lambda expresssion:
```
function (lambda expression)([l1 : parent_frame], l2 : ptr, l3 : long, l4 : ulong, l5 : long, l6 : ulong) : struct<16>
[...]
	i18 = load_relative_int(l3, 0)
	l19 = expand_int(i18)
	i22 = load_relative_int(l5, 0)
	l23 = expand_int(i22)
	l26 = l19 - l23
	return_struct_from_regs(l26, 1)
[...]
end
```

### x64 machine code
Using the `--dump-asm` flag to dump assembly of the lambda expresssion:
```asm
function (lambda expression)(ptr, long, ulong, long, ulong) : struct<16>
    7fb7e050a260:	8b 12                	mov    (%rdx),%edx
    7fb7e050a262:	48 63 c2             	movslq %edx,%rax
    7fb7e050a265:	45 8b 00             	mov    (%r8),%r8d
    7fb7e050a268:	4d 63 c0             	movslq %r8d,%r8
    7fb7e050a26b:	49 2b c0             	sub    %r8,%rax
    7fb7e050a26e:	ba 01 00 00 00       	mov    $0x1,%edx
    7fb7e050a273:	c3                   	retq
```

### More example code
There are examples including the usage of Types, Structs, Arrays, Threading and many more in
the [examples](examples/) directory of this repository. The most interresting ones are listed here:

- [pi](examples/pi.ptrs) and [circle](examples/circle.ptrs) Basic mathematic expressions and loops
- [fork](examples/fork.ptrs) Using posix functions for creating child processes
- [array](examples/array.ptrs) and [bubblesort](examples/bubblesort.ptrs) Basic array usage
- [struct](examples/struct.ptrs) Basic struct usage
- [threads](examples/threads.ptrs) Using libpthread
- [gtk](examples/gtk.ptrs) Using GTK for creating a window with a clickable button.
- [window](examples/window.ptrs) Using libSDL for creating X windows. (Example orginally by [@Webfreak001](https://github.com/WebFreak001))

## Installing
Pointerscript uses [libjit](https://www.gnu.org/software/libjit/) which is included in the repository as a submodule.
```bash
#Install dependencies (this might differ if you are not using debian)
# everything below apart from git and build-essential is required by libjit
sudo apt install git build-essential bison flex autoconf automake libtool texinfo

#Recursively clone the repository
git clone --recursive https://github.com/M4GNV5/PointerScript

#Compile...
cd PointerScript
make -j4 #-j specifies the number of tasks to run in parallel

#Done! PointerScript is at ./bin/ptrs
bin/ptrs --help
```

## License
[EUPL v1.2](LICENSE.txt)

> Copyright (C) 2020 Jakob Löw (jakob@löw.com)
>
> Licensed under the EUPL, Version 1.2 or - as soon they will be approved by the European
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
