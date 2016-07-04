# PointerScript
Scripting language with pointers and native library access.

##Example code
```javascript
//this program prints the fibonacci sequence from 0 to n
//n will be read from stdin

//import printf for output and scanf for input
import printf, scanf from "libc.so.6";

//read fibonacci count from stdin
var count = 0;
printf("Fibonacci count: ");
scanf("%d", &count);

var curr = 1;
var old = 0;
for(var i = 0; i <= count; i++)
{
	printf("Fibonacci %d = %d\n", i, curr);

	var tmp = old + curr;
	old = curr;
	curr = tmp;
}
```

##Language

###Standard Library
PointerScript has no standard-library. You can use all C libraries using the built-in ffi (importstatement),
however there are a couple of useful libraries (networking, regexp, etc.) in [this repo](https://github.com/M4GNV5/PtrsStuff)

###More example code
There are many examples including usage of Types, Structs, Arrays, Threading and many more in the [examples](examples/) directory of this repository. The most interresting ones are listed here:

- [types](examples/types.ptrs) Using typeof and type<...>
- [array](examples/array.ptrs) and [bubblesort](examples/bubblesort.ptrs) Basic array usage
- [struct](examples/struct.ptrs) Basic struct usage
- [threads](examples/threads.ptrs) Using libpthread (or generally native functions that take function pointer arguments) with Pointerscript
- [window](examples/window.ptrs) Using libSDL for creating X windows. (Example orginally by [@Webfreak001](https://github.com/WebFreak001))

###Performance
Generally PointerScript runs faster than many other scripting languages due to the absence of a Garbage Collector and the usage of native libraries.
You can create call graphs for the code examples in this repo using:
```bash
sudo apt-get install valgrind graphviz python3
curl -O ../gprof2dot.py https://raw.githubusercontent.com/jrfonseca/gprof2dot/master/gprof2dot.py
./measureExamples.sh
```
Currently the main time eaters are lookahead (parser) for small scripts without expensive loops and ptrs_scope_get for scripts with long loops

###Grammar
Most of PointerScript is similar to Javascript and/or C so only specialities will be listed here for a full Documentation see [LanguageDoc.md](LanguageDoc.md)
```javascript
//import native functions or variables from other PointerScript files
//leaving out the from part searches for the function in standard library
//e.g. 	import printf, scanf;
//		import pthread_create, pthread_join from "libpthread.so.0";
//		import myfunc, myvar from "./myotherfile.ptrs";
importstatement 		: 'import' argumentlist [ 'from' expression ] ';'
						;

//e.g.	var x = 42;		//noting unusual here
//		var y[x];		//creates an array of x variables on the stack
//		var z{x};		//creates an array of x bytes on the stack
variabledefinition		: 'var' identifier [ '=' expression ] ';'
						| 'var' identifier '[' expression ']' ';'
						| 'var' identifier '{' expression '}' ';'
						;

//e.g. cast<int>3.14 == 3
castexpression			: 'cast' '<' typename '>' expression
						;

//for use with typeof
//e.g. typeof foo == type<int>
typeexpression			: 'type' '<' typename '>' expression
						;

typename				: 'undefined'
						| 'int'
						| 'float'
						| 'native'
						| 'pointer'
						| 'function'
						| 'struct'
						;

constantexpression		: 'true' 		//type int, value 1
						| 'false'		//type int, value 0
						| 'null'		//type pointer, value 0
						| 'NULL'		//type native, value 0
						| 'VARSIZE'		//type int, value size of a variable (currently 24)
						| 'undefined'	//type undefined
						;

//see examples/struct.ptrs
structstatement			: 'struct' '{' memberdefinitionlist '}' ';'
						;
memberdefinitionlist	: memberdefinition [ memberdefinitionlist ]
						|
						;
memberdefinition		: identifier [ '=' expression ] ';'
						| identifier '[' expression ']' ';'
						| identifier '{' expression '}' ';'
						| 'operator' op '(' argumentdefinitonlist ')' body
						| identifier '(' argumentdefinitonlist ')' body
						;

//in function definitions you can define argument default values
//e.g. function foo(bar = 42 * 3112) {}
argumentdefinitonlist	: argumentdefiniton [ ',' argumentdefinitonlist ]
						|
						;
argumentdefiniton		: identifier [ '=' expression ]
						;

```
