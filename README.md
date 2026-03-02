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
		* `gcc version 13.2.0 (x86_64-posix-seh-rev0, Built by MinGW-Builds project)`
	* The above are from http://winlibs.com/
	* `Python 3.13.0`

1) Build the compiler
```powershell
g++ Masfix.cpp -o Masfix.exe
```

1) Run in interpreting mode  
_Interpreter works even without gcc setup._
```powershell
Masfix --interpret tests\basic-test.mx
test.py quick
```

1) Native compilation (assemble + link + run)  
_This sequence should help you set up your enviroment._
```powershell
Masfix --keep-asm --verbose tests\basic-test.mx
gcc -c -o tests\basic-test.obj tests\basic-test.s
gcc -nostartfiles -Wl,-e,_start -lkernel32 -o tests\basic-test.exe -g tests\basic-test.obj
tests\basic-test.exe
echo %ERRORLEVEL%
test.py run
```

1) Normal usage
```powershell
Masfix -r <.mx-file-to-run>
```

### IDE setup (VS Code)
#### Masfix syntax highlighting
- I use `x86 and x86_64 Assembly` vscode extension
- set vscode to auto-assign masfix files to this extension in `settings.json`:
```json
"files.associations": {
	"*.mx": "asm-intel-x86-generic"
}
```

#### Masfix code running
- We recommend `Code Runner` extension with:
```json
"code-runner.executorMapByFileExtension": {
	".mx": "Masfix -r",
}
```


# Getting started
- intended Masfix file extension - `.mx`
- whitespace insensitive
- single line comments use `;`
- one instruction per line
- macro system directives start with `%`

## Explore the repo
- [Code examples](./examples)
	- check out newest Masfix code standart [here](./examples/project_euler/multiples-3or5.mx)
	- or compare same code in [plain assembly](./examples/fibonacci_asm_only.mx) and [with macro system](./examples/fibonacci_modern.mx)
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
%include "memory"
%include "math"
%include "procedures"
%include "control"

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


# Inspiration & background
### Tsoding
- The idea of developing a programming language first appeared to me when watching [Tsoding's development of _Porth_ language](https://gitlab.com/tsoding/porth) ([YouTube playlist](https://www.youtube.com/playlist?list=PLpM-Dvs8t0VbMZA7wW9aR3EtBqe2kinu4)).  
- Concrete vision of **Masfix** came in early 2023 from his other project: [Tula](https://github.com/tsoding/tula).  

### NAND to Tetris
- great resource for hardware & software design
- building a processor and software stack from only basic building blocks
- [YouTube](https://www.youtube.com/playlist?list=PLrDd_kMiAuNmllp9vuPqCuttC1XL9VyVh)
- Masfix's memory layout & [segments](./std/core/procedures.mx#L153) are inspired by their VM's memory layout

The main goal of this project is to explore how programming language internals work by implementing them from scratch and discovering the various challenges that emerge along the way.  

> Proudly by __Michal-Martinek__
