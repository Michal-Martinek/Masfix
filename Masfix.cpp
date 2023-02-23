#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <assert.h>
using namespace std;

void genInstr(ofstream& outFile, string instr) {
	string instrName = instr.substr(0, instr.find(' '));
	string value = instr.substr(instr.find(' ') + 1, instr.size());
	
	// head pos - rbx
	outFile << "	; " << instrName << ' ' << value << '\n';
	if (instrName == "mov") {
		
		outFile << "	mov rbx, " << value << '\n';
	} else if (instrName == "str") {
		outFile << "	mov WORD [2*rbx+cells], " << value << '\n';
	} else {
		assert(("Unknown instruction", false));
	}

}

void generate(vector<string>& instrs, string outFileName="out.asm") {
	ofstream outFile;
	outFile.open(outFileName);

	outFile << 
		"extern ExitProcess@4\n"
		"global _start\n"
		"\n"
		"section .text\n"
		"_start:\n";


	for (string instr : instrs) {
		genInstr(outFile, instr);
	}
	outFile << 
		"\n	; exit(0)\n"
		"	mov rcx, 0\n"
		"	call ExitProcess@4\n"
		"\n"
		"section .bss\n"
		"	cells: resw 1024\n";
	outFile.close();
}

int main() {
	vector<string> instrs = {"mov 0", "str 1", "mov 1", "str 25", "mov 2", "str 65535"};
	generate(instrs);
}
