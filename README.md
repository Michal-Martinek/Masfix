# Masfix — Macro‑First, Assembly‑Like Language

Masfix builds high‑level features from the ground up with a powerful macro system on top of minimal, suffix‑based instructions that compile directly to x64.

- Masfix stands for **Macro ASsembly sufFIX**:
	- Pervasive __macro__ system: express control flow, namespaces, and build the language in itself.
		> Ever wondered how classes were implemented?
	- Low level assembly: based on simple instructions 
	- Suffix semantics: instruction meaning is specified by their suffixes.


## Architecture Overview
- Execution model resembles a [Turing machine](https://en.wikipedia.org/wiki/Turing_machine)
	- single read/write head moving along memory
	- __16 bit__ wide architecture
- Direct compilation to x64 assembly
	- developed for x64 Windows-Intel system
  
> Work in progress — This project may change at any time. Some features may be unimplemented or inconsistent with docs.

<!-- 
## Check out
* [Code examples?](./examples)
* [And the macro system?](#macro-system)
-->


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
_You can theoretically use ONLY interpreting mode to avoid nasm/gcc setup._  
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

# Language specifications
<!-- TODO recommended file ext -->
Masfix ignores all whitespace  

<!-- TODO footer? 2026 by Michal-Martinek -->