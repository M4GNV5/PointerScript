#Content
- [Usage](#usage)
	- [Types](#types)
	- [Constants](#constants)
	- [Structs](#structs)
	- [Variable Arguments](#variablearguments)
- [Operators](#operators)
	- [Binary](#binaryoperators)
	- [Prefixed](#prefixedoperators)
	- [Suffixed](#suffixedoperators)
- [Statements](#statements)
	- [ExpressionStatement](#expressionstatement)
	- [DefinitionStatement](#definitionstatement)
	- [ArrayDefinitionStatement](#arraydefinitionstatement)
	- [VarArrayDefinitionStatement](#vararraydefinitionstatement)
	- [StructVariableDefinition](#structvariabledefinition)
	- [ConstDefinition](#constdefinition)
	- [ImportStatement](#importstatement)
	- [ScopeStatement](#scopestatement)
	- [TryCatchStatement](#trycatchstatement)
	- [ThrowStatement](#throwstatement)
	- [FunctionStatement](#functionstatement)
	- [StructStatement](#structstatement)
	- [DeleteStatement](#deletestatement)
	- [SwitchStatement](#switchstatement)
	- [ForEachStatement](#foreachstatement)
	- [ControlStatements](#controlstatements)
- [Expressions](#expressions)
	- [CallExpression](#callexpression)
	- [ArrayExpression](#arrayexpression)
	- [VarArrayExpression](#vararrayexpression)
	- [ArrayStackExpression](#arraystackexpression)
	- [VarArrayStackExpression](#vararraystackexpression)
	- [StringFormatExpression](#stringformatexpression)
	- [NewExpression](#newexpression)
	- [NewStackExpression](#newstackexpression)
	- [MapExpression](#mapexpression)
	- [MapStackExpression](#mapstackexpression)
	- [MemberExpression](#memberexpression)
	- [IndexExpression](#indexexpression)
	- [SliceExpression](#sliceexpression)
	- [ExpandExpression](#expandexpression)
	- [AsExpression](#asexpression)
	- [CastBuiltinExpression](#castbuiltinexpression)
	- [CastExpression](#castexpression)
	- [CastStackExpression](#caststackexpression)
	- [IdentifierExpression](#identifierexpression)
	- [ConstantExpression](#constantexpression)
	- [BinaryExpression](#binaryexpression)
	- [PrefixedExpression](#prefixedexpression)
	- [SuffixedExpression](#suffixedexpression)

#Usage

##Types

###Type Usage
Checking if a variable has a specific type, can be done via `typeof` and `type<...>`.
The latter one will be replaced by a constant integer (the type) at parse time
(comparing integer is generally faster than comparing strings like with JavaScript's typeof)
```js
var myVal = //...
if(typeof myVal == type<int>)
	//...
```

Converting a variable to an `int` or `float` or `native`.
Casting to native will allocate memory on the stack and place a string representation in it.
```js
var myVal = cast<float>"3.14";
var myString = cast<native>myVal;
```

Changing the type of a variable. Note: this will not touch the actual value.
```js
//by default all native functions return an int
var ptr = as<native>malloc(1024);
```

###Type List
| Name | Name in C | Description |
|------|-----------|-------------|
| `undefined` | - | A not defined value |
| `int` | `int64_t` | 64 bit integer |
| `float` | `double` | 64 bit IEEE Float |
| `native` | `char *` | Pointer to a byte sequence or native function |
| `pointer` | `ptrs_var_t *` | Pointer to another variable |
| `function` | `ptrs_function_t *` | A PointerScript function |
| `struct` | `ptrs_struct_t *` | A PointerScript struct |

##Constants
| Name | Type | Value |
|------|------|-------|
| `true` | `int` | 1 |
| `false` | `int` | 0 |
| `NULL` | `native` | 0 |
| `null` | `pointer` | 0 |
| `VARSIZE` | `int` | Size of a variable |
| `PTRSIZE` | `int` | Size of a pointer |
| `undefined` | `undefined` | - |
| `NaN` | `float` | NaN |
| `PI` | `float` | pi |
| `E` | `float` | e |

##Structs
Structs are as powerful as classes in other languages (they support fields, functions, overlods, getters/setters, ...).
Here is a example of basic/common struct usage.
For things like operator overloading etc. see [struct.ptrs](examples/struct.ptrs) in the examples directory.
```js
struct Request
{
	host;
	port;
	error = false;
	buff{1024}; //byte array

	constructor(host = "localhost", port = 80)
	{
		this.host = host;
		this.port = port;
	}
	destructor()
	{
		//...
	}

	get data
	{
		if(this.error)
			throw "Trying to get the data of a failed request";
		return this.buff;
	}

	execute()
	{
		//...
	}
};

//create a new instance of Request
//alternatively you could use
//	var req : Request("m4gnus.de"); //this would allocate memory for 'req' on the stack
var req = new Request("m4gnus.de");

//call the execute method
req.execute();
//print the data
printf("data = %s\n", req.data);

//free the previousely allocated memory and call the destructor
delete req;
```

##Variable Arguments
PointerScript uses a C#/Java like approach optionally converting variable arguments to an array.
```js
function sum(values...)
{
	//as values is a normal array you can get its size using 'sizeof'
	var result = 0;
	for(var i = 0; i < sizeof values; i++)
	{
		result += values[i];
	}

	return result;
}
```
You can also pass the arguments to another function by extending the array using the `...args` syntax.
Note: this works for any array, **not** only arrays received via varargs.
```js
function printfln(fmt, args...)
{
	printf(fmt, ...args);
	printf("\n");
}
```

#Operators

##BinaryOperators
| Precedence | Operator | Description | Associativity |
|------------|----------|-------------|---------------|
| 1 | `= += -= *= /= %= <<= >>= &= ^= |=` | Assignment operators | Right-to-Left |
| 2 | `?:` | Ternary | Left-to-Right |
| 3 | `||` | Logical OR | Left-to-Right |
| 4 | `^^` | Logical XOR | Left-to-Right |
| 5 | `&&` | Logical AND | Left-to-Right |
| 6 | `|` | Bitwise OR | Left-to-Right |
| 7 | `^` | Bitwise XOR | Left-to-Right |
| 8 | `&` | Bitwise AND | Left-to-Right |
| 9 | `== != === !==` | (Typesafe-) Comparasion operators | Left-to-Right |
| 10 | `<= >= < >` | Comparasion operators | Left-to-Right |
| 11 | `instanceof` | Instanceof operator | Left-to-Right |
| 11 | `in` | Has-property operator | Left-to-Right |
| 12 | `<< >>` | Shifting operators | Left-to-Right |
| 13 | `+ -` | Addition, subtraction | Left-to-Right |
| 14 | `* / %` | Multiplication, division, division remainder | Left-to-Right |

##PrefixedOperators
| Operator | Description |
|------------|----------|
| `++ --` | Increment, decrement |
| `!` | Logical NOT |
| `~` | Bitwise NOT |
| `typeof` | Type of |
| `sizeof` | Size of |
| `&` | Address of |
| `*` | Dereference |
| `+ -` | Unary plus, minus |

##SuffixedOperators
| Operator | Description |
|------------|----------|
| `++ --` | Increment, decrement |

#Statements
##ExpressionStatement
```js
//Expression ';'
printf("%d", 42);
```

##DefinitionStatement
Creates a variable on the stack with an optional start value
```js
//'var' Identifier [ '=' Expression ] ';'
var foo;
var bar = "Hello";
var tar = 42 * 3112;
```

##ArrayDefinitionStatement
Creates an array of bytes on the Stack, optionally initialized with values from a string or an array literal.
```js
//'var' Identifier '{' Expression '}' ';'
var foo{32};

//	'var' Identifier '{' [ Expression ] '}' '=' String ';'
var foo{} = "Hello World!";
var bar{128} = "Ahoi!";

//'var' Identifier '{' [ Expression ] '}' '=' '{' ExpressionList '}' ';'
var foo{} = {31 * 12, 666};
var bar{32} = {42, 1337};
```

##VarArrayDefinitionStatement
Creates an array of variables on the Stack, optionally initialized with values from an array literal.
```js
//'var' Identifier '[' Expression ']' ';'
var foo[32];

//'var' Identifier '[]' [ Expression ] '}' '=' '[' ExpressionList ']' ';'
var foo[] = [31 * 12, 3.14, "Ahoi"];
var bar[32] = [42, 13.37, foo];
```

##StructVariableDefinition
Creates a struct on the stack.
```js
//'var' Identifier ':' Identifier '(' ExpressionList ')' ';'
var bar : Foo(42, 666);
```

##ConstDefinition
Creates a parse-time variable with a constant value
```js
//'const' Identifier '=' Expression ';'
const SEEK_SET = 0;
const SEEK_CUR = 1;
const SEEK_END = 2;
```

##ImportStatement
Imports variables/functions from native libraries or from other PointerScript files. If the from part is left out the functions will be searched in the default native library search order.
```js
//'import' IdentifierList [ 'from' Expression ] ';'
import printf, scanf;
import pthread_create, pthread_join from "libpthread.so.0";
import name, fibo from "otherFile.ptrs";
```
```js
//otherFile.ptrs
var name = "Hugo";
function fibo(val)
{
	return fibo(val - 1) + fibo(val - 2);
}
```

##ScopeStatement
Variables and stack allocations within a scoped statement won't be available outside the statement. Please note that all statements (except `foreach`) dont create a scope by themselves so doing stack allocations within a loop (e.g. by doing `var buff{1024};`) is probably a bad idea
```js
//'scoped' '{' StatementList '}'
var myPublicVar = 42;

scoped {
	var myHiddenVar = "supersecret";
	var myData{1024};
}

//the 1024 bytes used by myData are available again here
```

##TryCatchStatement
Executes the try body and catches any error (including signals) passing an error message, a backtrace string and source position information to the catch block. The catch block does not have to define identifiers for all 5 arguments.
```js
//'try' Statement 'catch' '(' IdentifierList ')' Statement
try
{
	//this will let us receive a SIGSEGV
	printf("%s", 42);
}
catch(error, backtrace, file, line, column)
{
	printf("%s\n%s\nAt %s:%d:%d", error, backtrace, file, line, column);
}
```

##ThrowStatement
```js
//throw Expression ';'
throw "I'm unhappy :(";
```

##FunctionStatement
Defines a function.
```js
//'function' Identifier '(' ArgumentDefinitionList ')' Block
function foo(a, b)
{

}
function bar(x, y = 42, z = foo(x, y))
{

}
//for information on how to use varargs see the Variable Arguments section
function tar(name, args...)
{

}
```
###ArgumentDefinition
Arguments will be set to the value to the caller provides or the default value if provided otherwise to `undefined`.
```js
//Identifier [ '=' Expression ]
```
The varargs argument will be set to an NULL terminated array of all additional arguments passed. This must be the last argument.
```js
//Identifier '...'
```

##StructStatement
Defines a struct.
```js
//'struct' Identifier '{' StructMemberDefinitionList '}' ';'
struct Person
{
	family;
	private _age = 18;
	name{128}; //128 bytes
	items[16]; //16 variables (16 * VARSIZE bytes)

	operator this + val
	{
		return _age + val;
	}

	constructor(familyCount)
	{
		this.family = malloc(familyCount);
	}
	destructor()
	{
		free(this.family);
	}

	myFunc(a, b)
	{
		return this.family[a + b];
	}

	get age
	{
		return _age;
	}
	set age
	{
		if(age < 0 || age > 200)
			throw "Invalid age";
		_age = value;
	}
}
```
###StructMemberDefinition
Note: the following code examples are only valid within a struct definition.
Modifiers:
- `private` wont be accesible through the `.` and `[]` operators
- `internal` wont be accesible from other files
- `public` (default) always accesible
- `static` all instances use the same value

####Variable member
```js
//Identifier [ '=' Expression ] ';'
age = 18;
```

####Typed member
```js
//Identifier ':' NativeTypeName
foo : long //char, int, long, longlong, uchar, uint, ulong, ulonglong
bar : i64 //i8, i16, i32, i64, u8, u16, u32, u64
tar : pointer
xar : single //single, double
zar : ptrdiff //ssize, size, intptr, uintptr, ptrdiff
```

####Array member
```js
//Identifier '{' ConstantExpression '}' ';'
name{128};
//Identifier '[' ConstantExpression ']' ';'
items[16];
```

####Function member
```js
//Identifier '(' ArgumentDefinitionList ')' '{' StatementList '}'
executeRequest(host = "localhost", port = 80)
{

}
```
####Getter/Setter
```js
//'get' Identifier '{' StatementList '}'
get age
{
	return _age;
}
//'set' Identifier '{' StatementList '}'
set age
{
	_age = value;
}
```

####Operator overload member
```js
//'constructor' '(' ArgumentDefinitionList ')' '{' StatementList '}'
constructor(x, y = 10, z = x + y)
{
	this.age = 18;
}
//'destructor' '(' ArgumentDefinitionList ')' '{' StatementList '}'
destructor()
{
}
//'operator' 'this' Operator Identifier '{' StatementList '}'
operator this / val
{
	return this.age / val;
}
//'operator' Identifier Operator 'this' '{' StatementList '}'
operator val / this
{
	return val / this.age;
}
//'operator' 'this' SuffixedUnaryOperator '{' StatementList '}'
operator this++
{

}
//'operator' PrefixedUnaryOperator 'this' '{' StatementList '}'
operator --this
{

}
//'operator' 'this' '.' Identifier '{' StatementList '}'
operator this.key
{
	printf("tried to get this.%s\n", key);
}
//'operator' 'this' '.' Identifier '=' Identifier '{' StatementList '}'
operator this.key = val
{
	printf("tried to set this.%s to %d\n", key, cast<int>val);
}
//'operator' '&' 'this' '.' Identifier '{' StatementList '}'
operator &this.key
{
	printf("tried to get the address of this.%s\n", key);
}
//'operator' 'this' '.' Identifier '(' IdentifierList ')' '{' StatementList '}'
operator this.key(foo, bar...)
{
	printf("calling this.%s\n", key);
}
//'operator' 'this' '[' Identifier ']' '{' StatementList '}'
operator this[key]
{
	printf("tried to get this[\"%s\"]\n", key);
}
//'operator' 'this' '[' Identifier ']' '=' Identifier '{' StatementList '}'
operator this[key] = val
{
	printf("tried to set this[\"%s\"] to %d\n", key, cast<int>val);
}
//'operator' '&' 'this' '[' Identifier ']' '{' StatementList '}'
operator &this[key]
{
	printf("tried to get the address of this.%s\n", key);
}
//'operator' 'this' '.' Identifier '(' IdentifierList ')' '{' StatementList '}'
operator this[key](foo, bar...)
{
	printf("calling this.%s\n", key);
}
//'operator' 'this' '(' ArgumentDefinitionList ')' '{' StatementList '}'
operator this(a, b) //will be called when the struct is called like a function
{
	return a + b;
}
//'operator' 'in' 'this' '{' StatementList '}'
operator foreach in this //overloads the foreach statement
{
	for(var i = 0; i < 16; i++)
	{
		//the yield expression will execute the foreach body like a function
		//if the yield expression returns a non-zero value the foreach body used 'break' or 'return'
		if(yield this.items[i], i)
			return;
	}
}
```

##DeleteStatement
Free's memory allocated with `new`. For structs it also calls the destructor
```js
//'delete' Expression ';'
var foo = new MyStruct(128);
delete foo;

var bar = new array[16];
delete bar;

var baz = new array{1024};
delete baz;
```

##SwitchStatement
Note: A break between one cases body and the next case is **not** necessary.
Also one case statement can have multiple cases seperated by a comma (all cases must be constants of type integer).
```js
//'switch' '(' Expression ')' '{' SwitchCaseBody '}'
//'case' ExpressionList ':'
//'default' ':'

var str;
switch(getchar())
{
	case 'a', 'b':
		str = "hey";
	case ':', 10:
		str = "ahoi";
	default:
		str = "hello";
}
```

##ForEachStatement
This statement can be used to iterate over arrays and structs.
Note that having more than one iterator is optional.
```js
//'foreach' '(' IdentifierList 'in' Expression ')' Statement

foreach(key, val in myStruct)
{

}
//or
var items[16];
foreach(i in items)
{

}
//or
foreach(index, val in items)
{

}
```

##ControlStatements
These are the same as in any other language (C, D, Javascript, ...)
```js
//'if' '(' Expression ')' Statement [ 'else' Statement ]
if(x >= 3)
	x = 0;

if(y == 3.14)
{
}
else
{
}

//'while' '(' Expression ')' Statement
while(x < 3)
	x++;

while(true)
{
}

//'do' Statement 'while' '(' Expression ')' ';'
do
{
	x--;
} while(x > 0);

//'for' '(' Statement ';' Expression ';' Expression ')' Statement
for(var i = 0; i < 10; i++)
{
	printf("z[%d] = %d\n", i, z[i]);
}

//'continue' ';'
continue;

//'break' ';'
break;

//'return' [ Expression ] ';'
return;
return atoi("42");
```

#Expressions

##CallExpression
Calls a function
```js
//Identifier '(' ExpressionList ')'
printf("x = %d\n", x)
```

##StringFormatExpression
Works like sprintf. Memory for the result string will be allocated on the stack
```js
//String '%' ExpressionList
var name = "Hugo";
"name = %s, age = %d" % name, 12;
```

##NewExpression
Creates an instance of a struct allocating its memory using malloc
```js
//'new' Identifier '(' ExpressionList ')'
new MyStruct(32);
```

##NewStackExpression
Creates an instance of a struct allocating its memory on the stack
```js
//'new_stack' Identifier '(' ExpressionList ')'
new_stack MyStruct(32);
```

##ArrayExpression
Creates an array. Memory will be allocated using `malloc`
```js
//'new' 'array' '{' Expression '}' [ '{' ExpressionList '}' ]
new array{1024};
new array{128} {'h', 'i', 0};
new array{} {'h', 'e', 'l', 'l', 'o', 0};
```

##VarArrayExpression
Creates a var-array. Memory will be allocated using `malloc`
```js
//'new' 'array' '[' Expression ']' [ '[' ExpressionList ']' ]
new array[16];
new array[8] [42, "hello", 31.12];
new array[] ["hi", 1337, PI, 9.11];
```

##ArrayStackExpression
Same as [ArrayExpression](#arrayexpression) but memory will be allocated on the stack
```js
//'new_stack' 'array' '{' Expression '}' [ '{' ExpressionList '}' ]
new_stack array{1024};
new_stack array{128} {'h', 'i', 0};
new_stack array{} {'h', 'e', 'l', 'l', 'o', 0};
```

##VarArrayStackExpression
Same as [VarArrayExpression](#vararrayexpression) but memory will be allocated on the stack
```js
//'new_stack' 'array' '[' Expression ']' [ '[' ExpressionList ']' ]
new_stack array[16];
new_stack array[8] [42, "hello", 31.12];
new_stack array[] ["hi", 1337, PI, 9.11];
```

##MapExpression
Creates a map of key->values in the form of a struct instance. This is a short form for
defining a struct, useful when defining constant data.
```js
//MapExpression		:=	'map' '{' MapEntryList '}'
//MapEntryList		:=	Identifier ':' Expression [ ',' MapEntryList ]
//					|	StringLiteral ':' Expression [ ',' MapEntryList ]

var escapes = map {
	n: '\n',
	"?": '\?',
	r: '\r',
	"\\": '\\'
};
```

##MapStackExpression
Same as [MapExpression](#mapexpression) but memory for the map is allocated on the stack
```js
//'map_stack' '{' MapEntryList '}'

map_stack {
	foo: 3,
	bar: function(a, b) { return a + b; }
	foobar: "1337",
}
```

##MemberExpression
```js
//Expression '.' Identifier
foo.bar
```

##IndexExpression
```js
//Expression '[' Expression ']'
foo["bar"] //for structs
items[7] //for arrays
```

##SliceExpression
`a[b .. c]` returns an array starting at `&a[b]` and length `c - b`. You can write `$`
instead of `b` meaning `b = 0`. Using `$` instead of `c` means `c = sizeof(a)`
```D
//Expression '[' Expression '..' Expression ']'
//Expression '[' '$' '..' Expression ']'
//Expression '[' Expression '..' '$' ']'
//Expression '[' '$' '..' '$' ']'

var foo[16];
//this will set bar to an array starting at '&foo[4]' with length '8'
var bar = foo[4 .. 12];
//this will result in an array starting at '&foo[2]' with length 'sizeof(foo) - 2' aka '14'
var tar = foo[2 .. $];
```

##ExpandExpression
```js
//'...' Expression
var args = [18, 666, "devil"];
printf("age: %d, evil: %d, sentence: %s", ...args);

var foo = [42, ...args, "hihi"];
```

##AsExpression
Note this will not convert any values, it will only change the type
```js
//'as' '<' TypeName '>' Expression
as<float>0x7fc00000
```

##CastBuiltinExpression
Converts a expression to a specific type. Currently only casting to int, float and native is supported.
```js
//'cast' '<' TypeName '>' Expression
cast<int>3.14
```

##CastExpression
Creates a struct of a specific type using the provided expression as the memory region.
This allows interop to C structs. This will **not** call the struct constructor
Note that this will allocate memory for struct metadata that has to be free'd using 'delete'
```js
//'cast' '<' Identifier '>' Expression

//see `man readdir` or http://man7.org/linux/man-pages/man3/readdir.3.html
struct dirent
{
	ino : u64;
	off : u64;
	reclen : ushort;
	type : uchar;
	name{256};
};
//... (you probably want to do a diropen first)
var entry = cast<dirent>readdir(dp);
//... (you probably want to use 'entry' here)
delete entry; //cast will allocate memory that can be freed using 'delete'
```

##CastStackExpression
Same as [CastExpression](#castexpression) but the memory for the struct metadata will be allocated
on the stack, thus a 'delete' is not necessary
```js
//'cast_stack' '<' Identifier '>' Expression
cast_stack<foo>bar;
```

##IdentifierExpression
```js
foo
```

##ConstantExpression
Note that constant mathematical expressions will be calculated during parse time.
```C
//String | Integer | Float
"Hello!"
'x' //char code not string
`Hello \ Whats up?
Are you fine?` //wysiwyg string
42
5f
3.14 * 5 + 7
```

##BinaryExpression
see
```js
//Expression Operator Expression
foo * 3
```

##PrefixedExpression
```js
//PrefixOperator Expression
-foo
```

##SuffixedExpression
```js
//Expression SuffixOperator
foo++
foo--
```
