# Masfix — Macro‑First, Assembly‑Like Language

Masfix builds high‑level features from the ground up with a powerful macro system on top of minimal, suffix‑based instructions that compile directly to x64.

- Masfix stands for **Macro ASsembly sufFIX**:
	- Pervasive __macro__ system: express control flow, namespaces, and build the language in itself.
		> Ever wondered how classes were implemented?
	- Low level assembly: based on simple instructions 
	- Suffix semantics: instruction meaning is specified by their suffixes.

<!-- TODO bullet point features overview -->

## Architecture Overview
- Execution model resembles a [Turing machine](https://en.wikipedia.org/wiki/Turing_machine)
	- single read/write head moving along memory
	- __16 bit__ wide architecture
- Direct compilation to x64 assembly
	- developed for x64 Windows-Intel system
  
> Work in progress — This project may change at any time. Some features may be unimplemented or inconsistent with docs.

# Setup
1) Install toolchain
	* `g++ (x86_64-posix-seh-rev0, Built by MinGW-Builds project) 13.2.0`
	* native compilation:
		* `NASM version 2.16.03 compiled on Apr 17 2024`
		* `gcc version 13.2.0 (x86_64-posix-seh-rev0, Built by MinGW-Builds project)`
	* The above are from http://winlibs.com/
	* `Python 3.13.0`

1) Build the compiler
```powershell
g++ Masfix.cpp -o Masfix.exe
```

1) Run in interpreting mode  
_Interpreter works even without nasm/gcc setup._
```powershell
Masfix --interpret tests\basic-test.mx
test.py quick
```

1) Native compilation (assemble + link + run)  
_This sequence should help you set up your enviroment._
```powershell
Masfix --keep-asm --verbose tests\basic-test.mx
nasm -fwin64 -g -F cv8 tests\basic-test.asm
gcc -nostartfiles -Wl,-e,_start -lkernel32 -o tests\basic-test.exe -g tests\basic-test.obj
tests\basic-test.exe
echo %ERRORLEVEL%
test.py run
```

1) Normal usage
```powershell
Masfix -r <.mx-file-to-run>
```

# Getting started
- intended Masfix file extension - `.mx`
- whitespace insensitive
- single line comments use `;`
- one instruction per line
- macro system directives start with `%`

## Explore the repo
- [Code examples](./examples)
	- check out newest Masfix code standart [here](TODO)
- [Standart library](./std)
- language docs: [assembly](./docs/assembly.md), [macro system](./docs/macro-system.md)


## Code example
```asm
mov 6    ; mov to addr 6
ldm      ; load r with value from memory
ld 42    ; load internal register (r) with immediate value
ld& 7    ; and r with 7
movart 3 ; addr += r * 3
strr     ; store r in memory at new addr


; include standart library with preprocessor
%include "std/memory"
%include "std/math"
%include "std/procedures"
%include "std/control"

; define preprocessor directives
%define divideBy 7
%macro callDiv_macro(local_param_divWith) {
	; macro body
	%stack:pushm() ; using macros
	%stack:push(%local_param_divWith)
	%call(divide)  ; call procedures
}

:loop              ; start of while loop
	outum          ; output memory
	%callDiv_macro(%divideBy)
	%stack:pop()

	%if {          ; control flow from std
		lrge 3,    ; if condition: load 1 -> r if r >= 3
		jmp loop   ; jmp back ^
	}
```

<!-- TODO footer? 2026 by Michal-Martinek -->