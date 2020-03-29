# Content
- [Usage](#usage)
	- [Commandline Arguments](#commandline-arguments)
	- [Types](#types)
	- [Constants](#constants)
	- [Structs](#structs)
	- [C Interop](#c-interop)
	- [Variable Arguments](#variable-arguments)
- [Operators](#operators)
	- [Binary](#binaryoperators)
	- [Prefixed](#prefixed-operators)
	- [Suffixed](#suffixed-operators)
	- [Operator result tables](#operator-result-tables)
- [Statements](#statements)
	- [ExpressionStatement](#expressionstatement)
	- [DefinitionStatement](#definitionstatement)
	- [ArrayDefinitionStatement](#arraydefinitionstatement)
	- [VarArrayDefinitionStatement](#vararraydefinitionstatement)
	- [ConstDefinition](#constdefinition)
	- [ImportStatement](#importstatement)
	- [ScopeStatement](#scopestatement)
	- [TryStatement](#trystatement)
	- [TryCatchStatement](#trycatchstatement)
	- [TryFinallyStatement](#tryfinallystatement)
	- [TryCatchFinallyStatement](#trycatchfinallystatement)
	- [ThrowStatement](#throwstatement)
	- [FunctionStatement](#functionstatement)
	- [StructStatement](#structstatement)
	- [DeleteStatement](#deletestatement)
	- [SwitchStatement](#switchstatement)
	- [ForEachStatement](#foreachstatement)
	- [ControlStatements](#controlstatements)
- [Expressions](#expressions)
	- [CallExpression](#callexpression)
	- [LambdaExpression](#lambdaexpression)
	- [StringFormatExpression](#stringformatexpression)
	- [SizeofExpression](#sizeofexpression)
	- [NewExpression](#newexpression)
	- [NewStackExpression](#newstackexpression)
	- [ArrayExpression](#arrayexpression)
	- [VarArrayExpression](#vararrayexpression)
	- [ArrayStackExpression](#arraystackexpression)
	- [VarArrayStackExpression](#vararraystackexpression)
	- [MapExpression](#mapexpression)
	- [MapStackExpression](#mapstackexpression)
	- [MemberExpression](#memberexpression)
	- [IndexExpression](#indexexpression)
	- [SliceExpression](#sliceexpression)
	- [IndexLengthExpression](#indexlengthexpression)
	- [ExpandExpression](#expandexpression)
	- [AsExpression](#asexpression)
	- [CastBuiltinExpression](#castbuiltinexpression)
	- [CastToStringExpression](#casttostringexpression)
	- [CastExpression](#castexpression)
	- [IdentifierExpression](#identifierexpression)
	- [ConstantExpression](#constantexpression)
	- [BinaryExpression](#binaryexpression)
	- [PrefixedExpression](#prefixedexpression)
	- [SuffixedExpression](#suffixedexpression)

# Usage

## Commandline Arguments
The syntax is `ptrs [options ...] <file> [script options ...]`.
Valid options are:

| Option | Argument | Description | Default |
|--------|----------|-------------|---------|
| `--help` | - | Print usage and argument information | - |
| `--array-max` | `size` | Set maximal allowed array size to 'size' bytes | `UINT32_MAX` |
| `--no-sig` | - | Do not listen to signals | `false` |
| `--no-aot` | - | Do not compile functions ahead of time | `false` |
| `--asmdump` | - | Dump the generated assembly instructions | `false` |
| `--error` | `file` | Set where error messages are written to | `/dev/stderr` |

The script options will be passed to the script in a global variable called `arguments`:
```js
//simple program that deletes a file
import remove, perror;

if(sizeof arguments != 1)
	throw "Usage: ptrs rm.ptrs <file>";

if(remove(arguments[0]) != 0)
	perror("Failed deleting file");
```

You can then run `ptrs rm.ptrs foo.txt` which will delete a file called `foo.txt` if it exists.
Please note that `ptrs foo.txt rm.ptrs` will not work, as script options must be provided
after the PointerScript source file.

## Types

### Type Usage
Checking if a variable has a specific type, can be done via `typeof` and `type<...>`.
The latter one will be replaced by a constant integer (the type) at parse time
(comparing integer is generally faster than comparing strings like with JavaScript's typeof)
```js
var myVal = //...
if(typeof myVal == type<int>)
	//...
```

Converting a variable to an `int` or `float` or `string`.
Casting to string will allocate memory on the stack and return a stringified
value of type `native`.
```js
let myVal = cast<float>"3.14";
let myInt = cast<int>myVal;
let myString = cast<string>(myInt * myVal);
```

Changing the type of a variable. Note: this will not touch the actual value.
```js
let x = 3;
let y = as<pointer>3;

//by default all native functions return an int
let ptr = malloc!native(1024);
```

### Type List
| Name | Name in C | Description |
|------|-----------|-------------|
| `undefined` | - | A not defined value |
| `int` | `int64_t` | 64 bit integer |
| `float` | `double` | 64 bit IEEE Float |
| `native` | `char *` | Pointer to a byte sequence or a function |
| `pointer` | `ptrs_var_t *` | Pointer to another variable |
| `struct` | `ptrs_struct_t *` | A PointerScript struct |

## Constants
| Name | Type | Value |
|------|------|-------|
| `true` | `int` | 1 |
| `false` | `int` | 0 |
| `NULL` | `native` | 0 |
| `null` | `pointer` | 0 |
| `undefined` | `undefined` | - |
| `NaN` | `float` | NaN |
| `Infinity` | `float` | Infinity |
| `PI` | `float` | pi |
| `E` | `float` | e |

## Structs
Structs are as powerful as classes in other languages (they support fields, functions, overloads, getters/setters, ...).
Here is an example of basic/common struct usage.
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
let req = new Request("m4gnus.de");

//call the execute method
req.execute();
//print the data
printf("data = %s\n", req.data);

//free the previousely allocated memory and call the destructor
delete req;
```

## C interop

### Functions
You can directly import C functions using the [ImportStatement](#importstatement) and then call them like normal functions:
```js
import puts, fopen, fprintf, fclose;
puts("Hello world!");

var fd = fopen("hello.txt", "w+");
fprintf(fd, "hi there!\nsome formats: %d %f %s", 42, 31.12, "ahoi");
fclose(fd);
```

### Structs
Structs can have typed members, thus you can use C functions that expect struct arguments:
```js
import printf, gettimeofday;

struct timeval
{
	sec : u64;
	usec : u64;
};

var time = new timeval();
gettimeofday(time, NULL);

printf("Seconds since epoch: %d\n", time.sec);
printf("Nanoseconds remainder: %d\n", time.usec);
printf("Milliseconds since epoch: %f\n", time.sec * 1000 + cast<float>time.usec / 1000);
```

When you have a function that returns `val` - a pointer to a struct you can also use `cast<structType>val` to create a struct of type `structType` using the memory pointed to by `val`

## Type list
| Type | Description | Name in C |
|------|-------------|-----------|
| `char short int long longlong` | signed integers | `char short int long` and `long long` |
| `uchar ushort uint ulong ulonglong` | unsigned integers | same as above but prefixed with `unsigned` |
| `i8 i16 i32 i64` | explicitly sized signed integers | `int8_t int16_t int32_t int64_t` |
| `u8 u16 u32 u64` | explicitly sized unsigned integers | `uint8_t uint16_t uint32_t uint64_t` |
| `ssize size intptr uintptr ptrdiff` | special types | `size_t ssize_t intptr_t uintptr_t ptrdiff_t`
| `single double` | floating point values | `float double` |
| `native` | pointer to u8's | `uint8_t *` |
| `pointer` | pointer to var's | `ptrs_var_t *` |


## Variable Arguments
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

# Operators

## Binary Operators
| Precedence | Operator | Description | Associativity |
|------------|----------|-------------|---------------|
| 1 | <code>= += -= *= /= %= <<= >>= &= ^= &#124;=</code> | Assignment operators | Right-to-Left |
| 2 | `?:` | Ternary | Left-to-Right |
| 3 | <code>&#124;&#124;</code> | Logical OR | Left-to-Right |
| 4 | `^^` | Logical XOR | Left-to-Right |
| 5 | `&&` | Logical AND | Left-to-Right |
| 6 | <code>&#124;</code> | Bitwise OR | Left-to-Right |
| 7 | `^` | Bitwise XOR | Left-to-Right |
| 8 | `&` | Bitwise AND | Left-to-Right |
| 9 | `== != === !==` | (Typesafe-) Comparasion operators | Left-to-Right |
| 10 | `<= >= < >` | Comparasion operators | Left-to-Right |
| 11 | `instanceof` | Instanceof operator | Left-to-Right |
| 11 | `in` | Has-property operator | Left-to-Right |
| 12 | `<< >> >>>` | Shifting operators | Left-to-Right |
| 13 | `+ -` | Addition, subtraction | Left-to-Right |
| 14 | `* / %` | Multiplication, division, division remainder | Left-to-Right |

## Prefixed Operators
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

## Suffixed Operators
| Operator | Description |
|------------|----------|
| `++ --` | Increment, decrement |

## Operator result Tables
The following table indicate the result type when using the corresponding operator(s).
Some types may be omitted, meaning they always cause an error when used with the specific operator.
For binary expressions in the form of `x op y` the types in the column headers
refer to the type of `x` and the types in the row headers refer to the type of `y`.
Operators which write to `x` and can be rewritten as `x = x op y` and are not listed in
the tables below as they work the same as their non-assigning counterpart.

### Addition
| `+`     | int     | float   | native  | pointer |
|---------|---------|---------|---------|---------|
| int     | int     | float   | native  | pointer |
| float   | float   | float   | *error* | *error* |
| native  | native  | *error* | *error* | *error* |
| pointer | pointer | *error* | *error* | *error* |

### Subtraction
| `-`     | int     | float   | native  | pointer |
|---------|---------|---------|---------|---------|
| int     | int     | float   | native  | pointer |
| float   | float   | float   | *error* | *error* |
| native  | *error* | *error* | int     | *error* |
| pointer | *error* | *error* | *error* | int     |

### Multiplication & Division
| `* /`   | int     | float   |
|---------|---------|---------|
| int     | int     | float   |
| float   | float   | float   |

### Integer only
| <code>&#124; ^ & >> << %</code> | int |
|---------------------------------|-----|
| int                             | int |

### Comparasion
All comparasion operators (`== != > < >= <=`) return either `true` aka `1` or `false` aka `0`.
Or in other words, their return type is always int. The type safe comparasion operators `=== !==`
return `false` when `typeof x != typeof y` otherwise they behave life `== !=`

### Prefixed Operators
| `++ -- ~` | int | float | native | pointer |
|-----------|-----|-------|--------|---------|
|           | int | float | native | pointer |

| `! typeof` | int | float | native | pointer | function | struct |
|------------|-----|-------|--------|---------|----------|--------|
|            | int | int   | int    | int     | int      | int    |

| `sizeof`   | native | pointer | struct |
|------------|--------|---------|--------|
|            | int    | int     | int    |

| `&`        | `&variable` | `&native[y]` | `&pointer[y]` |
|------------|-----------|------------|-------------|
|            | pointer   | native     | pointer     |

| `*`        | native | pointer   |
|------------|--------|-----------|
|            | int    | *unknown* |

| `+ -`      | int | float |
|------------|-----|-------|
|            | int | float |

### Suffixed Operators
| `++ --` | int | float | native | pointer |
|-----------|-----|-------|--------|---------|
|           | int | float | native | pointer |

# Statements
## ExpressionStatement
```js
//Expression ';'
printf("%d", 42);
```

## DefinitionStatement
Defines a variable, optionally initializing it with a start value.
```js
//'var' Identifier [ '=' Expression ] ';'
var foo;
var bar = "Hello";
var tar = 42 * 3112;
```

## ArrayDefinitionStatement
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

## VarArrayDefinitionStatement
Creates an array of variables on the Stack, optionally initialized with values from an array literal.
```js
//'var' Identifier '[' Expression ']' ';'
var foo[32];

//'var' Identifier '[]' [ Expression ] '}' '=' '[' ExpressionList ']' ';'
var foo[] = [31 * 12, 3.14, "Ahoi"];
var bar[32] = [42, 13.37, foo];
```

## ConstDefinition
Creates a parse-time variable with a constant value
```js
//'const' Identifier '=' Expression ';'
const SEEK_SET = 0;
const SEEK_CUR = 1;
const SEEK_END = 2;
```

## ImportStatement
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

Wildcard imports can be used when you need many functions from a library that all start with the same prefix so you don't have to write them all down manually.
```js
import curl_* from "libcurl.so";
var ctx = curl_easy_init();
curl_easy_setopt(ctx, 10002/*CURLOPT_URL*/, "https://pointerscript.org");
curl_easy_perform(ctx);
curl_easy_cleanup(ctx);
```

## ScopeStatement
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

## TryStatement
Executes the try block ignoring any error (including signals)
```js
//'try' Statement

try
{
	throw "This is ignored";
	printf("this is never executed\n");
}

//this will let us receive a SIGSEGV (that will be ignored)
try printf("%s", 42);
```

## TryCatchStatement
Executes the try block and catches any error (including signals) passing an error message, a backtrace string and source position information to the catch block. The catch block does not have to define identifiers for all 5 arguments.
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

## TryFinallyStatement
Executes the try block, if no error occurs the finally body will be executed. When an error occurs
in the try block, the finally block will be executed and the error will be re-thrown.
This can be used to close ressources or free memory.
Optionally the finally statement can take one argument that will be set to the return value of the try block or `undefined` if the try block does not have a return statement.
```js
//'try' Statement 'finally' [ '(' Identifier ')' ] Statement

var color = new array[] [255, 0, 64];
try
{
	riskyFunction(color);
}
finally
{
	delete color;
}

try
{
	return 42;
}
finally(ret)
{
	freeRessources();

	//you can either return 'ret' or something completely different
	return ret;
}
```

## TryCatchFinallyStatement
This combines the [TryCatchStatement](#trycatchstatement) and the [TryFinallyStatement](#tryfinallystatement)
```js
//'try' Statement 'catch' '(' IdentifierList ')' Statement 'finally' [ '(' Identifier ')' ] Statement

var fd = fopen("data.txt", "w");
try
{
	riskyStuff(fd);
}
catch(err, trace)
{
	printf("%s\n%s", err, trace);
}
finally
{
	//the finally block will *always* be executed, no matter if there was an exception or not
	//this makes sure 'fd' is always closed
	fclose(fd);
}
```

## ThrowStatement
```js
//throw Expression ';'
throw "I'm unhappy :(";
```

## FunctionStatement
Defines a function.
```js
//'function' Identifier '(' ArgumentDefinitionList ')' '{' StatementList '}'
function foo(a, b) { /* ... */ }
function bar(x, y = 42, z = foo(x, y)) { /* ... */ }
function foobar(m, _, n) { /* ... */ }
function tar(name, args...) { /* ... */ }
```
### ArgumentDefinition
Arguments will be set to the value the caller provides or the default value if provided otherwise to `undefined`.
```js
//Identifier [ '=' Expression ]
```
`_` means that this argument will be ignored
```js
//'_'
```
The varargs argument will be set to an array of all additional arguments passed. This must be the last argument.
For information on how to use varargs see the [Variable Arguments](#variable-arguments) section
```js
//Identifier '...'
```

## StructStatement
Defines a struct.
```js
//'struct' Identifier '{' StructMemberDefinitionList '}' ';'
struct Person
{
	family;
	private _age = 18;
	name{128}; //128 bytes
	items[16]; //16 variables (16 * sizeof var bytes)

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
### StructMemberDefinition
Note: the following code examples are only valid within a struct definition.
Modifiers:
- `private` wont be accesible through the `.` and `[]` operators
- `internal` wont be accesible from other files
- `public` (default) always accesible
- `static` all instances use the same value

#### Variable member
```js
//Identifier [ '=' Expression ] ';'
age = 18;
```

#### Typed member
```js
//Identifier ':' NativeType
foo : long //char, int, long, longlong, uchar, uint, ulong, ulonglong
bar : i64 //i8, i16, i32, i64, u8, u16, u32, u64
tar : pointer
xar : single //single, double
zar : ptrdiff //ssize, size, intptr, uintptr, ptrdiff
```

#### Array member
```js
//Identifier '{' ConstantExpression '}' ';'
name{128};
//Identifier '[' ConstantExpression ']' ';'
items[16];
```

#### Function member
```js
//Identifier '(' ArgumentDefinitionList ')' '{' StatementList '}'
executeRequest(host = "localhost", port = 80)
{

}
```
#### Getter/Setter
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

#### Operator overload member
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

/*
	Note that the following overloads both with with foo.bar and foo["bar"]
	even though their syntax may suggest they only work with the latter.
*/

//'operator' 'this' '[' Identifier ']' '{' StatementList '}'
operator this[key]
{
	printf("tried to get this.%s\n", key);
}
//'operator' 'this' '[' Identifier ']' '=' Identifier '{' StatementList '}'
operator this[key] = val //here key and val can be named however you like.
{
	printf("tried to set this.%s to %d\n", key, cast<int>val);
}
//'operator' '&' 'this' '[' Identifier ']' '{' StatementList '}'
operator &this[key]
{
	printf("tried to get the address of this.%s\n", key);
}
//'operator' 'this' '.' Identifier '(' ArgumentDefinitionList ')' '{' StatementList '}'
operator this[key](foo, bar...)
{
	printf("calling this.%s\n", key);
}

//'operator' 'this' '(' ArgumentDefinitionList ')' '{' StatementList '}'
operator this(a, b) //will be called when the struct is called like a function
{
	return a + b;
}
//'operator' 'foreach' 'in' 'this' '{' StatementList '}'
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

## DeleteStatement
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

## SwitchStatement
Note: A break between one cases body and the next case is **not** necessary.
Also one case statement can have multiple cases seperated by a comma (all cases must be constants of type integer).
A case may also be a range between two values.
```js
//SwitchStatement   := 'switch' '(' Expression ')' '{' SwitchCaseList '}'
//SwitchCase        := 'case' CaseList ':' StatementList
//                  |  'default' ':' StatementList

//CaseList          := IntegerConstant [ ',' CaseList ]
//                  |  IntegerConstant '..' IntegerConstant [ ',' CaseList ]

var str;
switch(getchar())
{
	case 'a', 'b':
		str = "hey";
	case 10, 128 .. 255:
		str = "ahoi";
	case '0' .. '9', 'f', 'e':
		str = "hóla";
	default:
		str = "hello";
}
```

## ForEachStatement
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

## ControlStatements
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

# Expressions

## CallExpression
Calls a function.

When calling a native function you can also specify the return type. This is necessary for functions that return floats and handy for functions that return pointers or signed integers that are not `int64_t`'s.
```js
//Identifier '(' ArgumentList ')'
//Identifier '!' TypeName '(' ArgumentList ')'

printf("x = %d\n", x)

pow!double(3.14, 7.2)
powf!single(31.12, 5.0)

dlerror!native()
atol!long("-42")
```

Arguments can be one of:
- `'_'` do not pass the argument (use default value)
- `Expression` pass a value
- `'...' Expression` expand an array as multiple arguments (expanding it)
```js
function foo(a = 3) { /* ... */ }
foo(_); //a will be 3

function bar(a) { /* ... */ }
bar(5); //a will be 5

function doSomething(x, y, z) { /* ... */ }
var args[] = [42, 1337, "foo"];
doSomething(...args); //x will be 42, y will be 1336 and z will be "foo"
```

## LambdaExpression
Function expression. For information about the argument syntax see [FunctionStatement](#functionstatement).
`LambdaArgumentList` is similar to `ArgumentDefinitionList` but does not support
default values.
```js
//'(' LambdaArgumentList ')' -> Expression
//'(' LambdaArgumentList ')' -> '{' StatementList '}'
//'function' '(' ArgumentDefinitionList ')' '{' StatementList '}' '{' StatementList '}'

var x = () -> 3;
var compar = (a, b) -> a - b;
var doStuff = (x, y) -> {
	doOtherStuff(x, y);
	return x;
};
var pow = function(base, exp = 2) {
	var result = 1;
	while(base > 0)
		result *= base--;
	return result;
}
```

## StringFormatExpression
Allocates a string of the required size on the stack, inserting variables and
expressions using `snprintf`. Without a custom sprintf-like format the value is
stringified and the format `%s` is used. If you provide a format the value is
passed to `snprintf` directly and your format is used (allowing you to e.g. print
the ASCII representation of integers).
```js
//inside a string:
//'$' Identifier
//'$' '{' Expression '}'
//'$' '%' Format '{' Expression '}'

var val = 3;
puts("val = $val"); //prints "val = 3"
puts("pid = ${getpid()}");
puts("valc = $%c{val + 'a'}"); //prints "valc = c"
```

## SizeofExpression
Returns the size of an expression, a C type or a variable.
You can optionally put braces around the type (like in C)
```js
//'sizeof' Expression
//'sizeof' [ '(' ] NativeType [ ')' ]
//'sizeof' [ '(' ] 'var' [ ')' ]

var foo[16];
var bar{} = "hello!";

sizeof foo; //16 same as sizeof(foo)
sizeof bar; //7  same as sizeof(bar), same as strlen(bar) + 1
sizeof(foo);
sizeof(bar);

sizeof int; //returns the size of the C 'int' type (usually 4 on amd64 computers)
sizeof(int);

sizeof var; //returns the size of a variable (usually 16)
sizeof(var);
```

## NewExpression
Creates an instance of a struct allocating its memory using `malloc`
```js
//'new' Identifier '(' ExpressionList ')'
new MyStruct(32);
```

## NewStackExpression
Creates an instance of a struct allocating its memory on the stack
```js
//'new_stack' Identifier '(' ExpressionList ')'
new_stack MyStruct(32);
```

## ArrayExpression
Creates an array. Memory will be allocated using `malloc`
```js
//'new' 'array' '{' Expression '}' [ '{' ExpressionList '}' ]
new array{1024};
new array{128} {'h', 'i', 0};
new array{} {'h', 'e', 'l', 'l', 'o', 0};
```

## VarArrayExpression
Creates a var-array. Memory will be allocated using `malloc`
```js
//'new' 'array' '[' Expression ']' [ '[' ExpressionList ']' ]
new array[16];
new array[8] [42, "hello", 31.12];
new array[] ["hi", 1337, PI, 9.11];
```

## ArrayStackExpression
Same as [ArrayExpression](#arrayexpression) but memory will be allocated on the stack
```js
//'new_stack' 'array' '{' Expression '}' [ '{' ExpressionList '}' ]
new_stack array{1024};
new_stack array{128} {'h', 'i', 0};
new_stack array{} {'h', 'e', 'l', 'l', 'o', 0};
```

## VarArrayStackExpression
Same as [VarArrayExpression](#vararrayexpression) but memory will be allocated on the stack
```js
//'new_stack' 'array' '[' Expression ']' [ '[' ExpressionList ']' ]
new_stack array[16];
new_stack array[8] [42, "hello", 31.12];
new_stack array[] ["hi", 1337, PI, 9.11];
```

## MapExpression
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

## MapStackExpression
Same as [MapExpression](#mapexpression) but memory for the map is allocated on the stack
```js
//'map_stack' '{' MapEntryList '}'

map_stack {
	foo: 3,
	bar: function(a, b) { return a + b; }
	foobar: "1337",
}
```

## MemberExpression
```js
//Expression '.' Identifier
foo.bar
```

## IndexExpression
```js
//Expression '[' Expression ']'
foo["bar"] //for structs
items[7] //for arrays
```

## SliceExpression
`a[b .. c]` returns an array starting at `&a[b]` and length `c - b`.
```D
//Expression '[' Expression '..' Expression ']'

var foo[16];
//this will set bar to an array starting at '&foo[4]' with length '8'
var bar = foo[4 .. 12];
```

## IndexLengthExpression
Only valid inside [IndexExpression](#indexexpression)s and [SliceExpression](#sliceexpression)s.
Returns the length of the array currently indexing.
```js
//'$'

var foo[6] = ["hello", 42, 1337, 31.12, 666, "ptrs"];

printf("%d\n", foo[$ - 2]); //prints 666
printf("%s\n", foo[$ - 1]); //prints ptrs

//sets bar to an array starting at `&foo[4]` with length '2'
var bar = foo[$ - 2 .. $]
//sets tar to an array starting at 'foo' with length '4'
var tar = foo[0 .. $ - 2];
```

## ExpandExpression
note that expression must be of type `pointer`
```js
//'...' Expression

var args = new array[] [18, "devil", 666];
var foo = new array[] [42, ...args, "hihi"];

//as seen under CallExpression
printf("age: %d, sentence: %s, evil: %d", ...args);

```

## AsExpression
Note this will not convert any values, it will only change the type
```js
//'as' '<' TypeName '>' Expression
as<float>0x7fc00000
```

## CastBuiltinExpression
Converts a value to either `int` or `float`.
```js
//'cast' '<' TypeName '>' Expression
cast<int>3.14 //returns 3
cast<int>"3.14" //returns 3
cast<float>"3.14" //returns 3.14
```

## CastToStringExpression
Converts an expression to a string (0-terminated byte sequence). Useful for printing a value.
The result is a read-only string with a lifetime bound to the lifetime of the expression.
```js
//'cast' '<' 'string' '>' Expression
cast<string>3.14 //returns "3.14"
cast<string>"foo" //returns "foo" (a copy on the stack)
cast<string>(new SomeStruct()) //returns "struct:0xc00000de" where 0xc00000de is the address of the struct
```

## CastExpression
Creates a struct of a specific type using the provided expression as the memory region.
This allows interop to C structs. This will **not** call the struct constructor
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
```

## IdentifierExpression
```js
foo
```

## ConstantExpression
Note that constant mathematical expressions will be calculated during compile time.
```C
//String | Integer | Float

"Hello!"
'x' //char code not string
`Hello \ Whats up?
Are you fine?` //wysiwyg string
42
5f
```

## BinaryExpression
see
```js
//Expression Operator Expression
foo * 3
```

## PrefixedExpression
```js
//PrefixOperator Expression
-foo
```

## SuffixedExpression
```js
//Expression SuffixOperator
foo++
```
