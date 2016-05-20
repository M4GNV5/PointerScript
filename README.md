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
Most of PointerScript is similar to Javascript and/or C so only specialities will be listed here:
```javascript
//import native functions
//leaving out the from part searches for the function in standard library
//e.g. 	import "printf", "scanf";
//		import "pthread_create", "pthread_join" from "libpthread.so.0";
//		import "myfunc", "myvar" from "./myotherfile.ptrs";
importstatement 		: 'import' argumentlist [ 'from' expression ] ';'
						;

//e.g.	var x = 42;			//noting unusual here
//		var y[1337 + x];	//creates an array of 1337 + x bytes on the stack
//		var z{3112 / x}		//creates an array of 3112 / x variables on the stack
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

//e.g.
/*
		import "printf", "free", "strncpy";

		struct Foo
		{
			x = 5;
			y[32];
			constructor(y)
			{
				strncpy(this.y, y, 31);
				this.y[32] = 0;
			}
			dump()
			{
				printf("%d %s\n", this.x, this.y);
			}
		};

		//you can then create an instance like in javascript
		var bar = new Foo("hello :3");
		bar.dump();
		//note that a struct instance is allocated memory that has to be free'd
		free(bar);
*/
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
