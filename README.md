Masfix is an assembly-like programming language with a powerful macro system enabling you to build up features found in high-level languages.

# About

Masfix stands for **Macro ASsembly sufFIX**. Masfix aims to be similar to assembly code, although it's instructions may be concretized with suffixes.  
**DISCLAIMER**: Both the language itself and this README are work in progress and may be changed at any time without any notice.  
Also the language specifications described here may not be fully implemented in the language or it might not accurately describe all present features of the language.  
  
* [How to set up the enviroment?](#setup)
* [Code examples?](./examples)
* [And the macro system?](#macro-system)

## Architecture description

The execution model resembles a [Turing machine](https://en.wikipedia.org/wiki/Turing_machine) in that it has a read-write head moving along a memory array.
The memory is divided into memory cells each being 16 bits wide. Additionally the head has an internal 16 bit general purpose register called **r**.
Masfix code is compiled directly into x64 nasm assembly, and it currently works only on x64 Intel Windows systems.

# Language specifications

The intended file extension for Masfix code is .mx, but any extension will work.  
Masfix ignores all whitespace, expecting at most one instruction per line.  
Line comments start with `;`. Masfix has no block comments.

## Registers

Masfix has 4 so called "registers":

* `h` - position of the read-write head, effecfivelly the current memory address
* `m` - contents of the currently selected memory cell (by the h register)
* `r` - the head's internal register, keeps its contents with head movements
* `p` - instruction pointer, contains the address of the currently executed instruction, increments after each instruction

## Instructions

### Basic instructions

These 4 instructions are used for setting the values of the [registers](#registers):
* `mov` - stores the value into the **h** register, efectively moving the head along the memory
* `str` - stores the value into the active memory cell
* `ld` - stores the value into the internal **r** register
* `jmp` - stores the value into the instruction pointer, effectively altering the execution flow

These are the only instructions that are allowed to have a [modifier](#modifier).

### Suffixes

Instructions are behaving like simple variable assignments. They have suffixes specifying the shape of the assignment.  
Namely the left side of the assignment is the destination register of the instruction  
and the right side called **target** can be an immediate value, register or an operation between a register and an immediate value.

For example:
```
instruction ; meaning 
ld 5 ; r = 5 
movm ; h = m 
strrs 2 ; m = r - 2 
ldamt 2 ; r += m * 2 
```

Let's discuss the fields of the last instruction "*ldamt 2*":

| example field | ld | a | m | t | 2 |
| --- | --- | --- | --- | --- | --- |
| **field type** | [instruction](#basic-instructions) | [modifier](#modifier) | [register](#basic-instructions) | [intermediate operation](#operations) | [immediate value](#immediate) |
| **description** | specifies instruction performed, here the destination will be **r** | operation modifying the destination | register used as argument | operation between register and immediate | constant value |

#### Instruction execution

1. load the **immediate value**, if present
1. load the **register** argument, if present
1. perform the **intermediate operation** between the **register** and the **immediate**, if used
1. perform the **modifier** operation, taking the instruction's **destination** as the first argument and the previously computed value as the second
1. store the result into the **destination**

#### Modifier
The modifier [operation](#operations) changes the destination by the target like `<destination> <operation>= <target>`,  
examples could be `lda 1 ; r += 1` or `strtrs 5 ; m *= r - 5`.  
Only the [4 basic instructions](#basic-instructions) are allowed a modifier.

#### Immediate
Immediate value supplied to the instruction. It's possible forms are: 
* A numerical value in the range [0, <the maximum value which can fit inside 16 bit unsigned int> = 65535]
* A [label](#labels) name, gets replaced with the address of the label

### Conditional instructions
These instructions apply some conditiotion against a condition register (**r** / **m**) and a second value.  
They take the form `<instr><cond-reg>?<cond><suffixes>*`, which means: instruction followed optionally with a letter specifying the destination register, then two letters specifying the condition and then standard suffixes.  
The default condition register is **r**.

#### Conditions

In conditions the numbers are interpreted as signed 2's complement numbers.

| cond | meaning | description |
| :---: | :---: | --- |
| `eq` | reg == sec | true if all bits are equal |
| `ne` | reg != sec | true if any bits are not equal |
| `lt` | reg <  sec | performs (reg - sec), then tests the sign bit of the result to be set |
| `le` | reg <= sec | performs (reg - sec), then tests the sign bit of the result to be set or the result equal to 0 |
| `gt` | reg >  sec | performs (reg - sec), then tests the sign bit of the result to be clear and the result not equal to 0 |
| `ge` | reg >= sec | performs (reg - sec), then tests the sign bit of the result to be clear |

#### Branches

Branches test the [condition register](#conditional-instructions) against 0.  
If its true that `<cond-reg> <condition> 0` then the instruction pointer is assigned the value of the **target**.

Examples:
```
beq <label> ; jumps to <label> if r == 0
bmge label2 ; jumps to label2 if m >= 0
brltm< 15 ; jumps to (m << 15) if r < 0
```

#### Condition load instructions

These perform a comparison between the [condition register](#conditional-instructions) and the **target** and store a boolean value *{0, 1}* into the destination.

* `l` - stores the result in **r**
* `s` - stores the result in **m**

Examples:
```
seq 56 ; m = (r == 56)
lmltra 7 ; r = m < (r + 7)
```

### Special instructions

These instructions are called "*special*" meaning they have restricted [suffixes](#suffixes). They are mainly IO instructions.

#### Output

* `outc` *aka* "output char" - prints the lowest 8 bits of the target as a char into the stdout
* `outu` *aka* "output unsigned" - prints the target as an unsigned int into the stdout

#### Input

The only suffix can be a register **r** / **m** specifying the destination, the default is **r**
* `inc` *aka* "input char" - blocks for input, eats a single char from the stdin and stores it in the destination.
* `ipc` *aka* "input peek char" - same as *inc*, but only peeks the char (doesn't consume it).
* `inu` *aka* "input unsigned" - blocks, eats all numerical chars, if it encounters a nonnumeric one it and stops and leaves it unconsumed.
	Constructs an unsigned int clamped to the wordsize out of the chars.
* `inl` *aka* "input new line" - consumes chars untill '\n' is eaten, does not influence any registers, has no suffixes nor immediate,
	it's main reason is to simplify handling of the '\n' or '\r\n' line breaks.

#### Others  
* `swap` - swaps the contents of **m** and **r**, has no suffixes nor immediate

Examples:  
Consider this example program when we feed it "xa065Ab\n-u \n" as stdin
```
inc ; r = 'x'
incm ; m = 'a'
inum ; m = 65
inl ; eats 'Ab\n'
inc ; r = '-'
ipc ; r = 'u', does not eat it
inu ; r = 0, leaves "u \n" unprocessed

swap ; r = 65, m = '-'

outcr ; prints 'A'
outcmt 2 ; prints ('-' * 2) = 90 = 'Z'
outurs 30 ; prints (65 - 30) = 35
```

### Operations

#### Arithmetic operations

Masfix has 3 basic numerical operations. Two different characters can be used to describe each operation,  
but we recommend using the letter in [instruction suffixes](#suffixes) and the special symbol in the [immediate value](#immediate).

* `a` *or* `+` - addition
* `s` *or* `-` - subtraction
* `t` *or* `*` - unsigned multiplication

#### Bitwise operations

* `&` - bitwise and
* `|` - bitwise or
* `^` - bitwise xor
* `<` - binary shift left
* `>` - binary shift right
* `.` - bit

**Note**: The last three operations are defined only for shifts of `0 - 16` (technically `0 - 63`) bits due to underlying implementation of bit shifts.

Examples:
```
At the start of each line r = 13 = 0x1101
ldr& 10 ; r = 0x1000
ldr| 17 ; r = 0x11101
ld^ 6   ; r = 0x1011
ld> 2   ; r = 0x11
ld< 3   ; r = 0x1101000
ld. 2   ; r = 1
ld. 1   ; r = 0
```

### Labels

Labels are used to simplify addressing of instructions in the source code. On use in [instruction immediates](#immediate), they are replaced with the exact address of the instruction directly following them.  
They need to be first on a line, **starting** with `:` followed with the label name.  
There are two predefined labels:
* `begin` - address 0
* `end` - address after the last instruction

## Macro system
As stated previously Masfix has a very powerful [macro](https://en.wikipedia.org/wiki/Macro_(computer_science)) system.  
Although Masfix is whitespace insensitive we recommend using common indentation rules.

### Token lists
Token lists are simply groups of tokens grouped by matching brackets `()`, `[]` or `{}`.
The bracket types are fully interchangeable, but we recommend using common bracket conventions as we do in all examples.  
Token lists are used exclusivelly in the macro system's directives.

### Defining directives
Defining directives define a [constant](#directive-define) or a [macro](#macro-directive).  
They always start with `%` at the start of a line.

#### Define directive
The `%define` directive is a way to define a global numeric constant like this: `%define a_name 15`  
The constant can be used like `%a_name`, which replaces the expression with the constant value.

#### Macro directive
The `%macro` directive is a way to define a global parametrized macro like this:
```
%macro mac_name(x, y) {
	ld %x ; some body
	outur
	outc %y
}
```  
The macro can be used like `%mac_name(5, 8)` on a separate line.  
This replaces the expression with the macro body as text substitutuion.  
The macro arguments must be numerical-value-reducible expressions.  
Any occurences of macro parameters (`%x` and `%y` in this case) in the macro body will get replaced with the supplied values (here 5 and 8).  

# Setup
This command sequence should help you set up your enviroment.  
`nasm` and `gcc` commands shown here are  used internally during compilation. 
```
# Testing of your setup
g++ Masfix.cpp -o Masfix
cd tests
..\Masfix --keep-asm --verbose basic-test.mx
nasm -fwin64 -g -F cv8 basic-test.asm
gcc -nostartfiles -Wl,-e,_start -lkernel32 -o basic-test.exe -g basic-test.obj
basic-test.exe
echo %ERRORLEVEL%
test.py run

# Normal operations
cd <Masfix-dir> && Masfix -r <.mx-file-to-run>
```

Here is a list of what I'm using:
* `g++ (x86_64-posix-seh-rev0, Built by MinGW-Builds project) 13.2.0`
* `NASM version 2.16.03 compiled on Apr 17 2024`
* `gcc version 13.2.0 (x86_64-posix-seh-rev0, Built by MinGW-Builds project)`
* The above are from http://winlibs.com/
* `Python 3.13.0`

<!---
TODOS

test examples
blobs like - 'build passing', 'code quality'

maybe add summary and collapses: https://github.com/jrblevin/markdown-mode/issues/658
^^ CAN BE DONE WITH PLAIN MD ^^
--->
