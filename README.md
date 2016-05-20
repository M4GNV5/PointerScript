# PointerScript
Wild mixture of C and Javascript

##Example code
```javascript
//this program prints the fibonacci sequence from 0 to n
//n will be read from stdin

//import printf for output and scanf for input
import "printf", "scanf" from "libc.so.6";

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

###Grammar
Most of PointerScript is similar to Javascript or C so only specialities will be listed here:
```BNF

//import native functions
//leaving out the from part searches for the function in standard library
//e.g. 	import "printf", "scanf";
//		import "pthread_create", "pthread_join" from "libpthread.so.0";
//		import "myfunc", "myvar" from "./myotherfile.ptrs";
importstatement 		: 'import' argumentlist [ 'from' expression ] ';'
						;

variabledefinition		: 'var' identifier [ '=' expression ] ';' //noting unusual here
						| 'var' identifier '[' expression ']' ';' //creates an array of `expression` bytes on the stack
						| 'var' identifier '{' expression '}' ';' //creates an array of `expression` variables on the stack
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
						| 'false'		//type int, value 1
						| 'null'		//type pointer, value 0
						| 'NULL'		//type native, value 0
						| 'VARSIZE'		//type int, value 1
						| 'undefined'	//type undefined
						;

structstatement			: 'struct' '{' memberdefinitionlist '}' ';'
						;
memberdefinitionlist	: memberdefinition [ memberdefinitionlist ]
						|
						;
memberdefinition		: identifier [ '=' expression ]
						| identifier '[' expression ']'
						| identifier '{' expression '}'
						| 'constructor' '(' argumentdefinitonlist ')' body
						| identifier '(' argumentdefinitonlist ')' body
						;

//in function definitions you can define argument default values
//e.g. function foo(bar = 42 * 3112) {}
argumentdefinitonlist	: argumentdefiniton [ argumentdefinitonlist ]
						|
						;
argumentdefiniton		: identifier [ '=' expression ]
						;

```
