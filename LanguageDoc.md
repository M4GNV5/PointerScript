#Content
- [Statements](#statements)
	- [ExpressionStatement](#expressionstatement)
	- [DefinitionStatement](#definitionstatement)
	- [ArrayDefinitionStatement](#arraydefinitionstatement)
	- [VarArrayDefinitionStatement](#vararraydefinitionstatement)
	- [StructVariableDefinition](#structvariabledefinition)
	- [ImportStatement](#importstatement)
	- [ScopeStatement](#scopestatement)
	- [TryCatchStatement](#trycatchstatement)
	- [ThrowStatement](#throwstatement)
	- [FunctionStatement](#functionstatement)
	- [StructStatement](#structstatement)
	- [DeleteStatement](#deletestatement)
	- [ControlStatements](#controlstatements)
- [Expressions](#expressions)
	- [CallExpression](#callexpression)
	- [ArrayExpression](#arrayexpression)
	- [VarArrayExpression](#vararrayexpression)
	- [StringFormatExpression](#stringformatexpression)
	- [NewExpression](#newexpression)
	- [MemberExpression](#memberexpression)
	- [IndexExpression](#indexexpression)
	- [AsExpression](#asexpression)
	- [CastExpression](#castexpression)
	- [IdentifierExpression](#identifierexpression)
	- [ConstantExpression](#constantexpression)
	- [BinaryExpression](#binaryexpression)
	- [PrefixedExpression](#prefixedexpression)
	- [SuffixedExpression](#suffixedexpression)

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
var bar{32} = [42, 13.37, foo];
```

##StructVariableDefinition
Creates a struct on the stack.
```js
//'var' Identifier ':' Identifier '(' ExpressionList ')' ';'
struct Foo
{
	x;
	y;
	constructor(a, b)
	{
		this.x = a;
		this.y = b;
	}
}
var bar : Foo(42, 666);
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
Variables within a scope statement wont be available outside and thus also wont use any memory anymore
```js
//'{' StatementList '}'
var myPublicVar = 42;
{
	var myHiddenVar = "supersecret";
}
```

##TryCatchStatement
Executes the try body and catches any error (including signals) passing an error message, a backtrace string and source position information to the catch block. The catch block does not have to define identifiers for all 5 arguments.
```js
//'try' Statement 'catch' '(' IdentifierList ')' Statement
try
{
	//this will let us receive a a SIGSEGV
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
struct MyStruct
{
	x;
	y = 3.14;
	a{128}; //128 bytes
	b[16]; //16 variables

	operator + (val)
	{
		return this.y + val;
	}

	constructor(len) //alias for operator new
	{
		this.x = malloc(len);
	}
	destructor() //alias for operator delete
	{
		free(this.x);
	}

	myFunc(a, b)
	{
		return this.y * a + b;
	}
}
```
###StructMemberDefinition
Variable member.
```js
//Identifier [ '=' Expression ] ';'
```
Array member.
```js
//Identifier '{' ConstantExpression '}' ';'
//Identifier '[' ConstantExpression ']' ';'
```
Function member.
```js
//Identifier '(' ArgumentDefinitionList ')' '{' StatementList '}'
```
Operator overload member. Note `constructor` is an alias for `operator new` and `destructor` for `operator delete`. Valid operators are:
- all Binary operators (`+`, `-`, ...)
- all prefixed operators (`++`, `~`, ...) excluding `&`and `*`
- all suffixed operators (`++`, `--`) note that `++` and `--` will be supplied with an argument set to `true` in case they are used suffixed
- `()` calling a struct
- `[]` getting a member from a struct using `["key"]`
- `.` getting a member from a struct using `.key`
- `cast` casting the array to another type
- `new` creating an instance either with the `NewExpression` or the `StructVariableDefinition`
- `delete` deleting the struct using the `DeleteStatement`
```js
//'operator' Operator '(' ArgumentDefinitionList ')' '{' StatementList '}'
//'constructor' '(' ArgumentDefinitionList ')' '{' StatementList '}'
//'destructor' '(' ArgumentDefinitionList ')' '{' StatementList '}'
```

##DeleteStatement
Calls the destructor of a given struct instance and free's its memory.
```js
var foo = new MyStruct(128);
delete foo;
```

##ControlStatements
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
	printf("z[%d] = %d\n", i, z[i]);

//'for' '(' [ 'var' ] Identifier 'in' Expression ')' Statement
var key;
for(key in myStruct)
{
}
for(var key in myStruct)
	printf("myStruct has member %s\n", key);

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
```js
//Identifier '(' ExpressionList ')'
printf("x = %d\n", x)
```

##ArrayExpression
```js
//'{' ExpressionList '}'
{'h', 'g' + 1}
```

##VarArrayExpression
```js
//'[' ExpressionList ']'
[3.14, "ahoi", 42]
```

##StringFormatExpression
```js
//String '%' ExpressionList
var name = "Hugo";
"name = %s, age = %d" % name, 12;
```

##NewExpression
```js
//'new' Identifier '(' ExpressionList ')'
new MyStruct(32);
```

##MemberExpression
```js
//Expression '.' Identifier
foo.bar
```

##IndexExpression
```js
//Expression '[' Expression ']'
foo["bar"]
```

##AsExpression
```js
//'as' '<' TypeName '>' Expression
as<float>0x7fc00000
```

##CastExpression
```js
//'cast' '<' TypeName '>' Expression
cast<int>3.14
```

##IdentifierExpression
```js
foo
```

##ConstantExpression
```C
//String | Integer | Float
"Hello!"
42
5f
3.14
```

##BinaryExpression
```js
//Expression Operator Expression
foo * 3
```
###Binary operators
| Precedence | Operator | Description | Associativity |
|------------|----------|-------------|---------------|
| 1 | `= += -= *= /= %= <<= >>= &= ^= |=` | Assignment operators | Right-to-Left |
| 2 | `?: instanceof` | Ternary, instanceof | Left-to-Right |
| 3 | `|| ^^` | Logical OR, logical XOR | Left-to-Right |
| 4 | `&&` | Logical AND | Left-to-Right |
| 5 | `|` | Bitwise OR | Left-to-Right |
| 6 | `^` | Bitwise XOR | Left-to-Right |
| 7 | `&` | Bitwise AND | Left-to-Right |
| 8 | `== != <= >= < >` | Comparasion operators | Left-to-Right |
| 9 | `<< >>` | Shifting operators | Left-to-Right |
| 10 | `+ -` | Addition, subtraction | Left-to-Right |
| 11 | `* / %` | Multiplication, division, division remainder | Left-to-Right |

##PrefixedExpression
```js
//PrefixOperator Expression
-foo
```
###Prefixed operators
| Operator | Description |
|------------|----------|
| `++ --` | Increment, decrement |
| `!` | Logical NOT |
| `~` | Bitwise NOT |
| typeof | Type of |
| `&` | Address of |
| `*` | Dereference |
| `+ -` | Unary plus, minus |

##SuffixedExpression
```js
//Expression SuffixOperator
foo++
foo--
```
###Suffixed operators
| Operator | Description |
|------------|----------|
| `++ --` | Increment, decrement |
