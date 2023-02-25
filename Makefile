all: compile-compiler without
without: run-compiler compile-asm run-exe

compile-compiler:
	g++ -std=c++17 Masfix.cpp -o Masfix

run-compiler:
	Masfix.exe

run-exe:
	out.exe

compile-asm:
	nasm -fwin64 out.asm -o out.obj
	ld out.obj c:\\Windows\\System32\\kernel32.dll -o out.exe -e _start
