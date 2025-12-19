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

