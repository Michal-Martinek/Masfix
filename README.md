# Masfix — Macro‑First, Assembly‑Like Language

Masfix builds high‑level features from the ground up with a powerful macro system on top of minimal, suffix‑based instructions that compile directly to x64.

- Masfix stands for **Macro ASsembly sufFIX**:
	- Pervasive __macro__ system: express control flow, namespaces, and build the language in itself.
		> Ever wondered how classes were implemented?
	- Low level assembly: based on simple instructions 
	- Suffix semantics: instruction meaning is specified by their suffixes.

> Work in progress — This project may change at any time. Some features may be unimplemented or inconsistent with docs.

## Architecture Overview
- Execution model resembles a [Turing machine](https://en.wikipedia.org/wiki/Turing_machine)
	- single read/write head moving along memory
	- __16 bit__ wide architecture
- Direct compilation to x64 assembly
	- developed for x64 Windows-Intel system
  
* [How to set up the enviroment?](#setup)
* [Code examples?](./examples)
* [And the macro system?](#macro-system)


# Language specifications

Masfix ignores all whitespace  

# Setup
These commands should help you set up your enviroment.  
You can theoretically use __interpretting mode only__.  
- compile compiler: `g++ Masfix.cpp -o Masfix`

#### interpretting mode
```
Masfix --interpret tests/basic-test.mx
test.py quick
```
#### native copilation
```
Masfix --keep-asm --verbose tests/basic-test.mx
nasm -fwin64 -g -F cv8 tests/basic-test.asm
gcc -nostartfiles -Wl,-e,_start -lkernel32 -o tests/basic-test.exe -g tests/basic-test.obj
tests\basic-test.exe
echo %ERRORLEVEL%
test.py run
```

#### normal operations
```
Masfix --run <.mx-file-to-run>
```

<!-- TODO move above? -->
## My tools
* `g++ (x86_64-posix-seh-rev0, Built by MinGW-Builds project) 13.2.0`
* `NASM version 2.16.03 compiled on Apr 17 2024`
* `gcc version 13.2.0 (x86_64-posix-seh-rev0, Built by MinGW-Builds project)`
* The above are from http://winlibs.com/
* `Python 3.13.0`
