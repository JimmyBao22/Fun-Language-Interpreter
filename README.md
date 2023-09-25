# Fun-Language-Interpreter

"Fun", a lightweight and Python-inspired language with support for conditionals, functions, recursion, loops, etc.

# Fun Programming Language Specification

## OPERATIONS AND PRECEDENCE
Operations higher in the following table have higher precedence than any operation below it. Operations at the same row in the list have equal precedence and are evaluated left to right. The operations are placed within curly braces in the below table but do not include them.

Let a and b be expressions that evaluate to unsigned 64 bit integers:

**Operations**
1) **{(a)}**
2) **{!a}**
3) **{a * b} , {a / b}, {a % b}**
4) **{a + b}, {a - b}**
5) **{a < b}, {a <= b}, {a > b}, {a >= b}**
6) **{a == b}, {a != b}**
7) **{a && b}**
8) **{a || b}**

**--------**

Every operation in this table returns an unsigned 64 bit integer. Every operation included in the previous spec has the exact same behavior.
The relational operators return 0 if the relation is false, and 1 if it is true.

(a) returns the value of a.

The logical operators (&&, ||, !) work like their counterparts in C. Non-zero operands are treated as “true”, and zero is treated as “false”. If the operation has a value of true, 1 is returned, and if the operation has a value of false, 0 is returned.

The operands of every binary operation are evaluated left to right.

Short-circuiting is NOT implemented. If the first operand of && is false, the second operand IS STILL evaluated. If the first operand of || is true, the second operand IS STILL evaluated.

/ and % by a zero second operand has undefined behavior.

## VARIABLES
Variables must abide by the naming guidelines of the first spec.

Variables names may not have the following names:

**Invalid Names**
1) **if**
2) **else**
3) **while**
4) **return**
5) **fun**

**-------**

Variables are declared, assigned, and reassigned by:
```python
<variableName> = <expression>
```
Variables defined outside of a function are global.

The use of undefined variables in expression has undefined behavior.

## FUNCTIONS
### Syntax For Defining A Function
```python
fun <functionName>(<parameter1>, <parameter2>, ..., <parameterN>) {
    <statement1>
    <statement2>
    ...
    <statementK>
}
```

### Syntax For Calling a Function
```python
<functionName>(<argument1>, <argument2>, ..., <argumentN>)
```
### Requirements
- May have multiple parameters separated by commas
- May have no parameters
- Cannot have parameters of the same name.
- The arguments to functions must be expressions that evaluate to an unsigned 64 bit integer.
- Every function returns a value. If a return statement is never hit in the function, the function returns 0.
- A return statement is written as: return <expression>
- Function calls by themselves do constitute a statement. So a call to a function f, f(1), for example, following another valid statement will not result in an error. Note that a literal like 40 would.
- Function calls whose number of arguments doesn't match the number of parameters the corresponding function has will result in an error.
The arguments of a function call are evaluated from left to right.
- Functions have a local scope, which consists of the parameters and any variables declared within the function
  - If a local variable has the same name as a global variable, any instance of the variable’s name in the function is considered to refer to the local variable.
  - Any variable declared within a function is considered local.
    - When an assignment statement is reached in a function:
      - If the LHS is a local variable
        - Reassign local variable
      - Else If the LHS is a global variable
        - Reassign global variable
      - Else
        - Make new local variable
- If there is a global variable and a parameter with the same name, any identifiers of that name are considered to refer to the parameter
- Functions CAN be recursive.
- If function 1 makes a call to function 2, function2 must have been defined before function 1 is called.
- Variables and Functions CAN Share names
- Functions CANNOT be redefined.
- Functions CANNOT be defined inside of other functions.
- Undefined functions CANNOT be called.
- Function names follow the exact same naming rules as variable names, defined in the previous spec, along with the restriction of not using special keywords that was introduced earlier in this spec. Additionally, functions may not be named “print”.

## IF STATEMENTS AND WHILE LOOPS
### If Statement Syntax
```python
if (<expression>) {
    <statement1>
    <statement2>
    ...
    <statementK>
}
```
### If Statement With Else Clause Syntax
```python
if (<expression>) {
    <statement1>
    <statement2>
    ...
    <statementK>
} else {
    <statementK+1>
    <statementK+2>
    ...
    <statementK+N>
}
```
### While Loop Syntax
```python
while (<expression>) {
    <statement1>
    <statement2>
    ...
    <statementN>
}
```
### Requirements
- A condition of 0 represents false, and the corresponding if / while body doesn’t run. If a condition is non zero, then the corresponding body does run.
- An else clause cannot appear without a corresponding preceding if clause.
- The body of if statements and while loops do not get their own scope separate from the global scope.

## ERRORS AND UNDEFINED BEHAVIOUR
Fun code that does not abide by the spec has undefined behavior.

Additionally, having statements/function declarations that span multiple lines or share the same line is also undefined behavior. Basically, new lines separate statements/fun decs from each other.

## PRINT
### Print Syntax
```python
print(<expression>)
```
and it prints the unsigned 64 bit value of the expression passed to it.

## ADDITIONAL NOTES
- The code is a sequence of statements. Statements include print, if, while, variable assignment, function declarations, function calls. Expressions by themselves are not statements and should not appear except as part of a statement.
  - f(5) is a valid statement (function call)
  - 5/2 is not a valid statement (it's just an expression)
  - x = 5/2 is a valid statement (variable assignment)
  - f(5) % 5 is not a valid statement (it's just an expression)
 
# Using The Compiler
## The command line interface:

./p3 <name>

The compiler (p3) reads a fun program from stdin and produces the compiled
output as x86-64 assembly to stdout.

You can compile the assembly to produce an executable

for example:

    ./p3 < t0.fun > t0.s
    gcc -o t0.run -static t0.s
    ./t0.run

The Makefile automates those tasks

### Adding Tests
Adding testcase, create 2 files:

       <name>.fun     contains the fun program
       <name>.ok      contains the expected output

### Generated files:

for each test:

    <test>.out    output from running the test
    <test>.diff   differences between the actual and expected output
    <test>.result pass/fail

### To run tests:

    make test

### To make the output less noisy:

    make -s test

### To run one test

    make -s t0.test

### To run by hand

    ./p1 < t0.fun

### File names used by the Makefile:

\<test\>.fun    &emsp;  -- fun program<br>
\<test\>.s      &emsp;  -- the equivalent x86-64 assembly<br>
\<test\>.run    &emsp;  -- the equivalent x86-64 executable<br>
\<test\>.out    &emsp;  -- the output from running \<test\>.run<br>
\<test\>.ok     &emsp;  -- the expected output<br>
\<test\>.diff   &emsp;  -- difference between \<test\>.out and \<test\>.ok<br>
\<test\>.result &emsp;  -- pass/fail<br>
\<test\>.time   &emsp;  -- how long it took to run \<test\>.run<br>
