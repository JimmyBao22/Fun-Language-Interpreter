#pragma once

// libc includes (available in both C and C++)
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Implementation includes
#include "mapcfunction.h"

// optional -> allows one to check if a slice/int was returned/exists
#define optional(type) struct { bool exists; type item; }

typedef optional(Slice) optionalSlice;
typedef optional(uint64_t) optionalInt;

typedef struct Interpreter {
    char* program;
    char* current;
    optionalInt functionReturn;
    UnorderedMap* currentSymbolTable;
    UnorderedMap* symbolTable;
    UnorderedFunctionMap* functionNameMap;
} Interpreter;

void fail(Interpreter* interpreter) {
    printf("failed at offset %ld\n", (size_t)(interpreter -> current - interpreter -> program));
    printf("%s\n", interpreter -> current);
    exit(1);
}

void endOrFail(Interpreter* interpreter) {
    while (isspace(*(interpreter -> current))) {
        interpreter -> current++;
    }
    if (*(interpreter -> current) != 0) {
        fail(interpreter);
    }
}

// skips past all white space
void skip(Interpreter* interpreter) {
    while (isspace(*(interpreter -> current))) {
        interpreter -> current++;
    }
}

// consumes to check if the current line matches str
bool consume(Interpreter* interpreter, char const *str) {
    skip(interpreter);

    size_t i = 0;
    while (true) {
      char const expected = str[i];
      char const found = interpreter -> current[i];
      if (expected == 0) {
        // survived to the end of the expected string 
        interpreter -> current += i;
        return true;
      }
      if (expected != found) {
        return false;
      }
      // assertion: found != 0
      i++;
    }
}

void consumeOrFail(Interpreter* interpreter, char const *str) {
    if (!consume(interpreter, str)) {
        fail(interpreter);
    }
}

// consume a variable 
optionalSlice consumeIdentifier(Interpreter* interpreter) {
    skip(interpreter);

    if (isalpha(*(interpreter -> current))) {
        char const *start = interpreter -> current;

        do {
            interpreter -> current++;
        } while(isalnum(*(interpreter -> current)));

        optionalSlice slice = { true, sliceConstructorLen(start, (size_t)(interpreter -> current - start)) };
        return slice;
    }
    else {
        optionalSlice slice = { false, sliceConstructorLen(0, 0) };
        return slice;
    }
}

// consume a number
optionalInt consumeLiteral(Interpreter* interpreter) {
    skip(interpreter);

    if (isdigit(*(interpreter -> current))) {
        uint64_t v = 0;

        do {
            v = 10 * v + ((*(interpreter -> current)) - '0');
            interpreter -> current++;
        } while (isdigit(*(interpreter -> current)));

        optionalInt opInt = { true, v };
        return opInt;
    }
    else {
        optionalInt opInt = { false, 0 };
        return opInt;
    }
}

// consume past a loop/if statement/function declaration if we want to skip it
void consumePast(Interpreter* interpreter) {
    int count = 1;
    while (count > 0) {
        if (consume(interpreter, "{")) {
            count++;
        }
        else if (consume(interpreter, "}")) {
            count--;
            if (count == 0) {
                break;
            }
        }
        else {
            interpreter -> current++;
        }
    }
}

// The plan is to honor as many C operators as possible with
// the same precedence and associativity
// e<n> implements operators with precedence 'n' (smaller is higher)

uint64_t expression(Interpreter* interpreter, bool effects, bool insideFunction);

uint64_t functionCall(Interpreter* interpreter, bool effects, Function* function);

// () [] . -> ...
uint64_t e1(Interpreter* interpreter, bool effects, bool insideFunction) {
    optionalSlice id = consumeIdentifier(interpreter);
    if (id.exists) {
        uint64_t v;
        if (consume(interpreter, "(")) {
            // check if there exists a function call
            Function* function = functionMapGet(interpreter -> functionNameMap, id.item);
            if (function != NULL) {
                v = functionCall(interpreter, effects, function);
            }
            else {
                fail(interpreter);
            }
        }
        else if (insideFunction && mapContains(interpreter -> currentSymbolTable, id.item)) {
            // utilize the local variable first
            v = mapGet(interpreter -> currentSymbolTable, id.item);
        }
        else {
            // if no local variable, use global variable
            v = mapGet(interpreter -> symbolTable, id.item);
        }
        return v;
    }
        
    optionalInt val = consumeLiteral(interpreter);
    if (val.exists) {
        return val.item;
    }

    if (consume(interpreter, "(")) {
        uint64_t v = expression(interpreter, effects, insideFunction);
        consume(interpreter, ")");
        return v;
    }

    fail(interpreter);
    return 0;
}

// ++ -- unary+ unary- ... (Right)
uint64_t e2(Interpreter* interpreter, bool effects, bool insideFunction) {
    bool neg = false;
    bool encounteredNeg = false;

    // logical not
    while (true) {
        if (consume(interpreter, "!")) {
            encounteredNeg = true;
            neg = !neg;
        }
        else {
            uint64_t v = e1(interpreter, effects, insideFunction);
            if (neg) {
                v = v > 0 ? 0 : 1;
            }
            if (encounteredNeg && v > 1) {
                v = 1;
            }
            return v;
        }
    }
}

// * / % (Left)
uint64_t e3(Interpreter* interpreter, bool effects, bool insideFunction) {
    uint64_t v = e2(interpreter, effects, insideFunction);

    while (true) {
        if (consume(interpreter, "*")) {
            v = v * e2(interpreter, effects, insideFunction);
        }
        else if (consume(interpreter, "/")) {
            uint64_t right = e2(interpreter, effects, insideFunction);
            v = (right == 0) ? 0 : v / right;
        }
        else if (consume(interpreter, "%")) {
            uint64_t right = e2(interpreter, effects, insideFunction);
            v = (right == 0) ? 0 : v % right;
        }
        else {
            return v;
        }
    }
}

// (Left) + -
uint64_t e4(Interpreter* interpreter, bool effects, bool insideFunction) {
    uint64_t v = e3(interpreter, effects, insideFunction);

    while (true) {
        if (consume(interpreter, "+")) {
            v = v + e3(interpreter, effects, insideFunction);
        }
        else if (consume(interpreter, "-")) {
            v = v - e3(interpreter, effects, insideFunction);
        }
        else {
            return v;
        }
    }
}

// << >>
uint64_t e5(Interpreter* interpreter, bool effects, bool insideFunction) {
    return e4(interpreter, effects, insideFunction);
}

// < <= > >=
uint64_t e6(Interpreter* interpreter, bool effects, bool insideFunction) {
    uint64_t v = e5(interpreter, effects, insideFunction);

    while (true) {
        if (consume(interpreter, "<=")) {
            uint64_t u = e5(interpreter, effects, insideFunction);
            v = (v <= u) ? 1 : 0;
        }
        else if (consume(interpreter, ">=")) {
            uint64_t u = e5(interpreter, effects, insideFunction);
            v = (v >= u) ? 1 : 0;
        }
        else if (consume(interpreter, "<")) {
            uint64_t u = e5(interpreter, effects, insideFunction);
            v = (v < u) ? 1 : 0;
        }
        else if (consume(interpreter, ">")) {
            uint64_t u = e5(interpreter, effects, insideFunction);
            v = (v > u) ? 1 : 0;
        }
        else {
            return v;
        }
    }
}

// == !=
uint64_t e7(Interpreter* interpreter, bool effects, bool insideFunction) {
    uint64_t v = e6(interpreter, effects, insideFunction);

    while (true) {
        if (consume(interpreter, "==")) {
            uint64_t u = e6(interpreter, effects, insideFunction);
            v = (v == u) ? 1 : 0;
        }
        else if (consume(interpreter, "!=")) {
            uint64_t u = e6(interpreter, effects, insideFunction);
            v = (v != u) ? 1 : 0;
        }
        else {
            return v;
        }
    }
}

// (left) &
uint64_t e8(Interpreter* interpreter, bool effects, bool insideFunction) {
    return e7(interpreter, effects, insideFunction);
}

// ^
uint64_t e9(Interpreter* interpreter, bool effects, bool insideFunction) {
    return e8(interpreter, effects, insideFunction);
}

// |
uint64_t e10(Interpreter* interpreter, bool effects, bool insideFunction) {
    return e9(interpreter, effects, insideFunction);
}

// &&
uint64_t e11(Interpreter* interpreter, bool effects, bool insideFunction) {
    uint64_t v = e10(interpreter, effects, insideFunction);

    while (true) {
        if (consume(interpreter, "&&")) {
            v = e10(interpreter, effects, insideFunction) && v;
        }
        else {
            return v;
        }
    }
}

// ||
uint64_t e12(Interpreter* interpreter, bool effects, bool insideFunction) {
    uint64_t v = e11(interpreter, effects, insideFunction);
    
    while (true) {
        if (consume(interpreter, "||")) {
            v = e11(interpreter, effects, insideFunction) || v;
        }
        else {
            return v;
        }
    }
}

// (right with special treatment for middle expression) ?:
uint64_t e13(Interpreter* interpreter, bool effects, bool insideFunction) {
    return e12(interpreter, effects, insideFunction);
}

// = += -= ...
uint64_t e14(Interpreter* interpreter, bool effects, bool insideFunction) {
    return e13(interpreter, effects, insideFunction);
}

// ,
uint64_t e15(Interpreter* interpreter, bool effects, bool insideFunction) {
    return e14(interpreter, effects, insideFunction);
}

uint64_t expression(Interpreter* interpreter, bool effects, bool insideFunction) {
    return e15(interpreter, effects, insideFunction);
}

bool statement(Interpreter* interpreter, bool effects, bool insideFunction);

// checks if a return statement is/was reached
bool checkReturn(Interpreter* interpreter, bool effects, bool insideFunction) {
    if (insideFunction && interpreter -> functionReturn.exists) {
        return true;
    }
    char* currentPointer = (interpreter -> current);
    optionalSlice s = consumeIdentifier(interpreter);
    if (insideFunction && s.exists && sliceEqualString(s.item, "return")) {
        // return the corresponding value
        optionalInt v = { true, expression(interpreter, effects, insideFunction) };
        interpreter -> functionReturn = v;
        return true;
    }

    // reset the pointer since no return was discovered
    interpreter -> current = currentPointer;
    return false;
}

// parse a while loop
bool whileStatement(Interpreter* interpreter, bool effects, bool insideFunction) {
    char* currentPointer = (interpreter -> current);
    while (true) {
        consumeOrFail(interpreter, "(");
        uint64_t v = expression(interpreter, effects, insideFunction);
        consumeOrFail(interpreter, ")");

        if (v == 0) {
            // skip over while loop
            consumeOrFail(interpreter, "{");
            consumePast(interpreter);
            return false;
        }
        else {
            // enter while loop
            consumeOrFail(interpreter, "{");
            while (!consume(interpreter, "}")) {
                if (checkReturn(interpreter, effects, insideFunction)) {
                    consumePast(interpreter);
                    return true;
                }

                if (!statement(interpreter, effects, insideFunction)) {
                    fail(interpreter);
                }
            }

            // reset the pointer to check the conditional again
            interpreter -> current = currentPointer;
        }
    }

    return false;
}

// performs the body of a function
uint64_t performFunction(Interpreter* interpreter, bool effects, Function* function) {    
    char* currentPointer = interpreter -> current;

    // move pointer to where the function was defined in order to do the function operations
    interpreter -> current = function -> pointer;

    uint64_t v = 0; 

    consumeOrFail(interpreter, "{");

    while (!consume(interpreter, "}")) {
        if (interpreter -> functionReturn.exists || checkReturn(interpreter, effects, true)) {
            // if a return was reached, set the return value and reset functionReturn in interpreter
            v = interpreter -> functionReturn.item;
            optionalInt val = { false, 0 };
            interpreter -> functionReturn = val;
            break;
        }
        if (!statement(interpreter, effects, true)) {
            fail(interpreter);
        }
    }

    if (interpreter -> functionReturn.exists) {
        v = interpreter -> functionReturn.item;
        optionalInt val = { false, 0 };
        interpreter -> functionReturn = val;
    }

    // set pointer back
    interpreter -> current = currentPointer;

    return v;
}

uint64_t functionCall(Interpreter* interpreter, bool effects, Function* function) {    
    // create a new local map for the current state
    UnorderedMap* previousSymbolTable = interpreter -> currentSymbolTable;
    UnorderedMap* currentSymbolTable = mapCreate();

    consume(interpreter, "(");
    uint64_t parameterCount = 0;

    // read in all parameters 
    while (!consume(interpreter, ")")) {
        uint64_t value = expression(interpreter, effects, true);
        // printSlice(function -> parameters[parameterCount]);
        // printf(" %ld \n", value);

        mapInsert(currentSymbolTable, function -> parameters[parameterCount], value);
        parameterCount++;
        consume(interpreter, ",");
    }
    
    if (parameterCount != function -> numParams) {
        fail(interpreter);
    }

    interpreter -> currentSymbolTable = currentSymbolTable;

    // special function print -> need to actually print the value returned
    if (sliceEqualString(function -> name, "print")) {
        uint64_t v = mapGet(interpreter -> currentSymbolTable, function -> parameters[0]);
        if (effects) {
            printf("%lu\n", v);
        }
        interpreter -> currentSymbolTable = previousSymbolTable;
        freeMap(currentSymbolTable);
        // print function default return is 0
        return 0;
    }

    uint64_t v = performFunction(interpreter, effects, function);

    // reset the currentSymbolTable to waht it was before the function call
    interpreter -> currentSymbolTable = previousSymbolTable;
    freeMap(currentSymbolTable);
    return v;
}

bool statement(Interpreter* interpreter, bool effects, bool insideFunction) {
    // printf("START\n%s\nEND\n\n", (interpreter -> current));

    if (consume(interpreter, "#")) {
        // this line is a comment, skip it
        while (interpreter -> current[0] != '\n') {
            interpreter -> current++;
        }
        return true;
    }

    if (checkReturn(interpreter, effects, insideFunction)) {
        return true;
    }

    optionalSlice id = consumeIdentifier(interpreter);

    if (!id.exists) {
        return false;
    }

    if (sliceEqualString(id.item, "if")) {
        // if ... 
        consumeOrFail(interpreter, "(");
        uint64_t v = expression(interpreter, effects, insideFunction);
        consumeOrFail(interpreter, ")");

        if (v == 0) {
            // skip over if statement
            consumeOrFail(interpreter, "{");
            consumePast(interpreter);

            // check if there is an else statement. If there is, enter the statement. If not, move pointer back and continue
            char* prevPointer = interpreter -> current;
            optionalSlice checkElse = consumeIdentifier(interpreter);            
            if (sliceEqualString(checkElse.item, "else")) {
                consumeOrFail(interpreter, "{");

                while (!consume(interpreter, "}")) {
                    if (checkReturn(interpreter, effects, insideFunction)) {
                        consumePast(interpreter);
                        return true;
                    }
                    if (!statement(interpreter, effects, insideFunction)) {
                        fail(interpreter);
                    }
                }
            }
            else {
                interpreter -> current = prevPointer;
            }
        }
        else {
            // enter if statement
            consumeOrFail(interpreter, "{");
            while (!consume(interpreter, "}")) {
                if (checkReturn(interpreter, effects, insideFunction)) {
                    consumePast(interpreter);
                    return true;
                }
                if (!statement(interpreter, effects, insideFunction)) {
                    fail(interpreter);
                }
            }

            // check for else statement
            char* prevPointer = interpreter -> current;
            optionalSlice checkElse = consumeIdentifier(interpreter);
            if (sliceEqualString(checkElse.item, "else")) {
                consumeOrFail(interpreter, "{");
                // skip the else
                consumePast(interpreter);
            }
            else {
                interpreter -> current = prevPointer;
            }
        }

        return true;
    }

    if (sliceEqualString(id.item, "while")) {
        // while ... 
        whileStatement(interpreter, effects, insideFunction);
        return true;
    }

    if (sliceEqualString(id.item, "else")) {
        // error, cannot have else without a preceding if statement
        fail(interpreter);
    }

    if (sliceEqualString(id.item, "fun")) {
        if (insideFunction) {
            // cannot define a function inside another function
            fail(interpreter);
        }

        // fun ... 
        optionalSlice functionName = consumeIdentifier(interpreter);
        Function* currentFunction = (Function*) (malloc(sizeof(Function)));
        currentFunction -> numParams = 0;

        if (functionName.exists) {
            currentFunction -> name = functionName.item;
        }
        else {
            fail(interpreter);
        }

        consume(interpreter, "(");
        char* beforePointer = interpreter -> current;

        // find the number of parameters in this function to malloc the parameter array
        while (!consume(interpreter, ")")) {
            optionalSlice parameterName = consumeIdentifier(interpreter);
            if (parameterName.exists) {
                currentFunction -> numParams++;
            }
            else {
                fail(interpreter);
            }
            consume(interpreter, ",");
        }

        currentFunction -> parameters = (Slice*) (malloc(sizeof(Slice) * currentFunction -> numParams));

        interpreter -> current = beforePointer;
        uint64_t i = 0;

        // consume parameters 
        while (!consume(interpreter, ")")) {
            optionalSlice parameterName = consumeIdentifier(interpreter);
            currentFunction -> parameters[i++] = parameterName.item;
            consume(interpreter, ",");
        }

        currentFunction -> pointer = interpreter -> current;

        // enter this function into the function map which maps the function name to the function struct
        functionMapInsert(interpreter -> functionNameMap, functionName.item, currentFunction);

        // skip past the function for now, only need to read in body of function when we call the function
        consumeOrFail(interpreter, "{");
        consumePast(interpreter);

        return true;
    }
    
    // check for invalid variable names
    if (sliceEqualString(id.item, "if") || sliceEqualString(id.item, "else") || sliceEqualString(id.item, "while") || 
            sliceEqualString(id.item, "return") || sliceEqualString(id.item, "fun")) {
        fail(interpreter);
    }

    if (consume(interpreter, "=")) {
        uint64_t v = expression(interpreter, effects, insideFunction);

        if (effects) {
            if (!insideFunction) {
                mapInsert(interpreter -> symbolTable, id.item, v);
            }
            else {
                /*
                    When an assignment statement is reached in a function:
                        If the LHS is a local variable
                            Reassign local variable
                        Else If the LHS is a global variable
                            Reassign global variable
                        Else
                            Make new local variable
                */
                if (mapContains(interpreter -> currentSymbolTable, id.item)) {
                    // update the local variable
                    mapInsert(interpreter -> currentSymbolTable, id.item, v);
                }
                else if (mapContains(interpreter -> symbolTable, id.item)) {
                    // update the global variable
                    mapInsert(interpreter -> symbolTable, id.item, v);
                }
                else {
                    // create a new local variable
                    mapInsert(interpreter -> currentSymbolTable, id.item, v);
                }
            }
        }

        return true;
    }
    else {
        // can have a stand-alone function call without doing (var) = (function call)
        Function* function = functionMapGet(interpreter -> functionNameMap, id.item);
        if (function != NULL) {
            functionCall(interpreter, effects, function);
        }
        else {
            fail(interpreter);
        }

        return true;
    }

    return false;
}

void statements(Interpreter* interpreter, bool effects) {
    while (statement(interpreter, effects, false));
}

void run(Interpreter* interpreter) {
    statements(interpreter, true);
    endOrFail(interpreter);
}

Function* createPrintFunction() {
    Function* currentFunction = (Function*) (malloc(sizeof(Function)));
    currentFunction -> name = sliceConstructorLen("print", 5);
    currentFunction -> numParams = 1;
    currentFunction -> parameters = (Slice*) (malloc(sizeof(Slice)));
    currentFunction -> parameters[0] = sliceConstructorLen(".", 1);
    return currentFunction;
}

Interpreter* interpreterConstructor(char* prog) {
    Interpreter* interpreter = (Interpreter*) (malloc(sizeof(Interpreter)));
    interpreter -> program = prog;
    interpreter -> current = prog;
    optionalInt v = { false, 0 };
    interpreter -> functionReturn = v;
    interpreter -> currentSymbolTable = mapCreate();
    interpreter -> symbolTable = mapCreate();
    interpreter -> functionNameMap = functionMapCreate();
    
    // create a default print function
    functionMapInsert(interpreter -> functionNameMap, sliceConstructorLen("print", 5), createPrintFunction());

    return interpreter;
}
