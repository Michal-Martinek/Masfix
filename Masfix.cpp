#include <iostream>
#include <string>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <vector>
#include <fstream>
#include <assert.h>
using namespace std;

// helpers ---------------------------------
 std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}
std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}
std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}
// structs -------------------------------
struct InstrToken {
	string instr;
	bool hasImm;
	string immediate;

	InstrToken(string instr, bool hasImm, string immediate) {
		this->instr = instr;
		this->hasImm = hasImm;
		this->immediate = immediate;
	}
	string toStr() {
		string s = instr;
		if (hasImm) {
			s = s.append(" ").append(immediate);
		}
		return s;
	}
};

vector<InstrToken> tokenize(ifstream& inFile) {
	string line;
	vector<InstrToken> tokens;
	
	while (getline(inFile, line)) {
		line = line.substr(0, line.find(';'));
		line = trim(line);
		if (line.size() > 0) {
			
			bool hasImm = false;
			string instr = "", imm = "";
			if (line.find(' ') != string::npos) {
				hasImm = true;
				instr = line.substr(0, line.find(' '));
				imm = line.substr(line.find(' ')+1);
				assert(imm.find(' ') == string::npos);
			} else 
				instr = line;
			InstrToken token(instr, hasImm, imm);
			tokens.push_back(token);
		}
	}
	return tokens;
}

void genInstr(ofstream& outFile, InstrToken instr) {	
	// head pos - rbx, internal r reg - rcx
	int imm = 0;
	if (instr.hasImm) imm = stoi(instr.immediate);
	outFile << "	; " << instr.toStr() << '\n';

	if (instr.instr == "mov") {
		outFile << "	mov rbx, " << imm << '\n';
	} else if (instr.instr == "str") {
		outFile << "	mov WORD [2*rbx+cells], " << imm << '\n';
	} else if (instr.instr == "ld") {
		outFile << "	mov rcx, " << imm << '\n';
	} else if (instr.instr == "outum") {
		outFile << "	mov ax, [2*rbx+cells]\n"
			"	call print_unsigned\n";
	} else if (instr.instr == "outur") {
		outFile << "	mov ax, cx\n"
			"	call print_unsigned\n";
	} else if (instr.instr == "outc") {
		outFile << 
			"	mov [stdout_buff], WORD " << imm << "\n"
			"	mov rdx, stdout_buff\n"
			"	mov r8, 1\n"
			"	call stdout_write\n";
	} else {
		assert(("Unknown instruction", false));
	}

}

void generate(vector<InstrToken>& instrs, string outFileName="out.asm") {
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
		"	push rcx\n"
		"	mov rcx, [stdout_fd] ; stdout fd\n"
		"	push 0 ; number of bytes written var\n"
		"	mov r9, rsp ; ptr to that var\n"
		"	push 0 ; OVERLAPPED struct null ptr\n"
		"	sub rsp, 32 ; call with shadow space\n"
		"	call WriteFile@20\n"
		"	add rsp, 32+8 ; get rid of the OVERLAPPED\n"
		"	pop rax\n"
		"	pop rcx\n"
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
		"	mov rdx, r9 ; buff addr\n"
		"	call stdout_write\n"
		"	ret\n"
		"\n"
		"global _start\n"
		"_start:\n"
		"	; initialization\n"
		"	call get_std_fds\n"
		"	xor rbx, rbx\n"
		"	xor rcx, rcx\n"
		"\n";
	


	for (InstrToken instr : instrs) {
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
	ifstream inFile;
	inFile.open("in.mx");
	vector<InstrToken> instrs = tokenize(inFile);
	generate(instrs);
}
