
# Assembly specification
## Registers

Masfix has 4 so called "registers":

* `h` - position of the read-write head, effecfivelly the current memory address
* `m` - contents of the currently selected memory cell (by the h register)
* `r` - general purpose internal register, keeps its contents with head movements
* `p` - instruction pointer, contains the address of the currently executed instruction, increments after each instruction

## Instructions

### Basic instructions

These 4 instructions are used for setting the values of [registers](#registers):
* `mov` - stores the value into the **h** register, efectively moving the head along the memory
* `str` - stores the value into the active memory cell
* `ld` - stores the value into the internal **r** register
* `jmp` - stores the value into the instruction pointer, effectively altering the execution flow


### Suffixes
Instructions behave like simple variable assignments.  
Suffixes specify shape of the assignment.  
Left side of the assignment is the destination register of the instruction.  
The right side (called **target value**) can take these forms:
- immediate value
- register
- operation between a register and an immediate value

#### Modifier
The modifier [operation](#operations) changes the destination by the target like `<destination> <operation>= <target>`,  
examples could be `lda 1 ; r += 1` or `strtrs 5 ; m *= r - 5`.  
Only the [4 basic instructions](#basic-instructions) are allowed a modifier.

#### Immediate
Immediate value supplied to the instruction. It's possible forms are: 
* A numerical value in the range [0, 65535] _(maximum of 16 bit unsigned int)_
* A [label](#labels) name, gets replaced with the address of the label

### Basic instruction examples
```
ld 5 ; r = 5
movm ; h = m
strrs 2 ; m = r - 2
ldamt 2 ; r += m * 2
```

Breakdown of last instruction's `ldamt 2` fields:

| field | ld _(load)_ | a _(add)_ | m _(memory)_ | t _(times)_ | 2 |
| --- | --- | --- | --- | --- | --- |
| **type** | [instruction](#basic-instructions) | [modifier](#modifier) | [register](#basic-instructions) | [intermediate operation](#operations) | [immediate value](#immediate) |
| **description** | specifies instruction performed, here the destination will be **r** | operation modifying the destination by target value | register used as argument | operation between register and immediate value | constant value |


### Instruction execution
- Instructions are generally evaluated from right to left:
1. load the **immediate value**, if present
1. load the **register** argument, if present
1. perform the **intermediate operation** between the **register** and the **immediate**, if used
1. perform the **modifier** operation, taking the instruction's **destination** as the first argument and the previously computed value as the second
1. store the result into the **destination**


### Conditional instructions
Test some condition between condition register (**r** / **m**) and **target value**.  
Condition register may be omitted - default is **r**.

#### conditinal instruction's fields
Example: `lmltra 7 ; r = m < (r + 7)`  
| field | l | m | lt  | r a 7 |
| --- | --- | --- | --- | --- |
| **type** | [conditional load](#condition-load-instructions) | [memory register](#registers) | [condition](#conditions) | [other suffixes](#suffixes) |
| **description** | loads condition result (0/1) to **r** | comparison LHS - memory (default **r**) | `<` - signed less than | suffixes forming the RHS of comparison |

#### Branches
Branches test the [condition register](#conditional-instructions) against 0.  
If the condition holds, then the instruction pointer is assigned the value of **target**.

Examples:
```
beq <label> ; jumps to <label> if r == 0
bmge label2 ; jumps to label2 if m >= 0
brltm< 15   ; jumps to (m << 15) if r < 0
            ; WARNING - the above doesn't mean "jump to 15 if r < m"!
```

#### Condition load instructions

These perform a comparison between the [condition register](#conditional-instructions) and **target** and store a boolean value *{0, 1}* into the destination.

* `l` - stores the result in **r**
* `s` - stores the result in **m**

Examples:
```
seq 56   ; m = (r == 56)
lmltra 7 ; r = m < (r + 7)
```

#### Conditions
Conditions compare two numbers either as signed or unsigned.

| cond | meaning | description |
| :---: | :---: | --- |
| `eq` | reg == sec | true if all bits are equal |
| `ne` | reg != sec | true if any bits are not equal |
| `lt` | reg <  sec | performs (reg - sec), then tests the sign bit of the result to be set |
| `le` | reg <= sec | performs (reg - sec), then tests the sign bit of the result to be set or the result equal to 0 |
| `gt` | reg >  sec | performs (reg - sec), then tests the sign bit of the result to be clear and the result not equal to 0 |
| `ge` | reg >= sec | performs (reg - sec), then tests the sign bit of the result to be clear |
| `ab` | reg > sec | unsigned above |
| `ae` | reg >= sec | unsigned above equal |
| `bl` | reg < sec | unsigned below |
| `be` | reg <= sec | unsigned below equal |

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
Operations perform aritmetic or bitwise operations between [registers](#registers) and [immediate value](#immediate).  
Operands "come in the same order" (_L/R_) as in the [instruction suffix](#suffixes).  
Their results are always constrained to 16 bits (even intermediate calculations)

#### Arithmetic operations
* `a` - addition
* `s` - subtraction
* `t` - unsigned multiplication

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
; Assume r = 13 = 0x1101 for each line
ldr& 10 ; r = 0x1000
ldr| 17 ; r = 0x11101
ld^ 6   ; r = 0x1011
ld> 2   ; r = 0x11
ld< 3   ; r = 0x1101000
ld. 2   ; r = 1
ld. 1   ; r = 0
```

### Labels

Labels are used to simplify addressing of instructions in the source code.  
When used in [instruction immediate](#immediate), they are replaced with the exact address of instruction directly following the label definition.  
Label definition needs to be first on line, **starting** with `:` followed by the label name.  
`:label <possible-instr>`

- predefined labels:
	* `begin` - address 0
	* `end` - address after the last instruction
