#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <assert.h>
using namespace std;

void genInstr(ofstream& outFile, string instr) {
	string instrName = instr.substr(0, instr.find(' '));
	string value = "";
	if (instrName != instr)
		value = instr.substr(instr.find(' ') + 1, instr.size());
	
	// head pos - rbx
	outFile << "	; " << instrName << ' ' << value << '\n';
	if (instrName == "mov") {
		outFile << "	mov rbx, " << value << '\n';
	} else if (instrName == "str") {
		outFile << "	mov WORD [2*rbx+cells], " << value << '\n';
	} else if (instrName == "outum") {
		outFile << "	mov ax, [2*rbx+cells]\n"
			"	call print_unsigned\n";
	} else {
		assert(("Unknown instruction", false));
	}

}

void generate(vector<string>& instrs, string outFileName="out.asm") {
	ofstream outFile;
	outFile.open(outFileName);

	outFile << 
		"extern ExitProcess@4\n"
		"extern GetStdHandle@4\n"
		"extern WriteFile@20\n"
		"\n"
		"section .text\n"
		"get_std_fds: ; prepares all std fds\n"
		"	mov rcx, -11 ; stdout fd\n"
		"	sub rsp, 32 ; call with shadow space\n"
		"	call GetStdHandle@4\n"
		"	add rsp, 32\n"
		"	mov [stdout_fd], rax\n"
		"	ret\n"
		"stdout_write: ; rdx - buff, r8 - number of bytes -> rax - number written\n"
		"	mov rcx, [stdout_fd] ; stdout fd\n"
		"	push 0 ; number of bytes written var\n"
		"	mov r9, rsp ; ptr to that var\n"
		"	push 0 ; OVERLAPPED struct null ptr\n"
		"	sub rsp, 32 ; call with shadow space\n"
		"	call WriteFile@20\n"
		"	add rsp, 32+8 ; get rid of the OVERLAPPED\n"
		"	pop rax\n"
		"	ret\n"
		"print_unsigned: ; n - rax\n"
		"	xor r8, r8 ; char count\n"
		"	mov r9, stdout_buff+255 ; curr buff pos\n"
		"	mov r10, 10 ; base\n"
		"print_unsigned_loop:\n"
		"	xor rdx, rdx\n"
		"	div r10\n"
		"	add rdx, '0'\n"
		"\n"
		"	inc r8\n"
		"	dec r9\n"
		"	mov [r9], dl\n"
		"\n"
		"	cmp rax, 0\n"
		"	jne print_unsigned_loop\n"
		"	\n"
		"	; TODO: save regs - not needed bcoz rbx is safe\n"
		"	mov rdx, r9 ; buff addr\n"
		"	call stdout_write\n"
		"	ret\n"
		"\n"
		"global _start\n"
		"_start:\n"
		"	call get_std_fds\n";
	


	for (string instr : instrs) {
		genInstr(outFile, instr);
	}
	outFile << 
		"\n"
		"	; exit(0)\n"
		"	mov rcx, 0\n"
		"	sub rsp, 32\n"
		"	call ExitProcess@4\n"
		"	hlt\n"
		"\n"
		"section .bss\n"
		"	cells: resw 1024\n"
		"	stdout_fd: resq 1\n"
		"	stdout_buff: resb 256\n";
	
	outFile.close();
}

int main() {
	vector<string> instrs = {"mov 0", "str 1", "mov 1", "str 65536", "mov 2", "str 65535", "mov 0", "outum", "mov 1", "outum", "mov 2", "outum"};
	generate(instrs);
}
