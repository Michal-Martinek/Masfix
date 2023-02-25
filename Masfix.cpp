#include <iostream>
#include <string>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <vector>
#include <fstream>
#include <math.h>
using namespace std;

// helpers ---------------------------------
string &ltrim(string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
			std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}
string &rtrim(string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(),
			std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}
string trim(string &s) {
	return ltrim(rtrim(s));
}
vector<string> splitOnWhitespace(string &s) {
	vector<string> splits;
	int start = 0;
	for (int i = 0; i < s.size(); ++i) {
		if (std::isspace(s.at(i))) {
			string chunk = s.substr(start, i - start);
			if (chunk.size()) splits.push_back(chunk);
			start = i+1;
		}
	}
	if (start < s.size()) {
		string chunk = s.substr(start);
		if (chunk.size()) splits.push_back(chunk);
	}
	return splits;
}
string deleteWhitespace(string s) {
	for (int i = 0; i < s.size();) {
		if (std::isspace(s.at(i)))
			s.erase(i, 1);
		else i++;
	}
	return s;
}

void check(bool cond, string message) {
	if (!cond) {
		cout << "ERROR: " << message << '\n';
		exit(1);
	}
}
// structs -------------------------------
struct InstrToken {
	string instr = "";
	bool hasImm = false;
	int immediate;
	
	InstrToken() {}
	InstrToken(string instr, bool hasImm, int immediate) {
		this->instr = instr;
		this->hasImm = hasImm;
		this->immediate = immediate;
	}
	string toStr() {
		string s = instr;
		if (hasImm) {
			s = s.append(" ").append(to_string(immediate));
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
			vector<string> splits = splitOnWhitespace(line);
			check(splits.size() <= 2, "Instruction has too many fields '" + line + "'");
			
			InstrToken token;
			token.instr = splits[0];
			token.hasImm = splits.size() == 2;
			if (token.hasImm) {
				int imm = stoi(splits[1]);
				check(to_string(imm) == splits[1], "Invalid instruction immediate '" + splits[1] + "'");
				check(0 <= imm && imm < (int)exp2(16), "Value of the immediate '" + to_string(imm) + "' is out of bounds");
				token.immediate = imm;
			}
			tokens.push_back(token);
		}
	}
	return tokens;
}

void genAssembly(ofstream& outFile, InstrToken instr) {	
	// head pos - rbx, internal r reg - rcx
	if (instr.instr == "mov") {
		outFile << "	mov rbx, " << instr.immediate << '\n';
	} else if (instr.instr == "str") {
		outFile << "	mov WORD [2*rbx+cells], " << instr.immediate << '\n';
	} else if (instr.instr == "ld") {
		outFile << "	mov rcx, " << instr.immediate << '\n';
	} else if (instr.instr == "outum") {
		outFile << "	mov ax, [2*rbx+cells]\n"
			"	call print_unsigned\n";
	} else if (instr.instr == "outur") {
		outFile << "	mov ax, cx\n"
			"	call print_unsigned\n";
	} else if (instr.instr == "outc") {
		outFile << 
			"	mov [stdout_buff], WORD " << instr.immediate << "\n"
			"	mov rdx, stdout_buff\n"
			"	mov r8, 1\n"
			"	call stdout_write\n";
	} else {
		check(false, "Unknown instruction: '" + instr.instr + "'");
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
	
	InstrToken instr;
	for (int i = 0; i < instrs.size(); ++i) {
		instr = instrs[i];
		outFile << "instr_" << i << ":\n";
		outFile << "	; " << instr.toStr() << '\n';
		genAssembly(outFile, instr);
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
		"	stdout_buff: resb 256\n"
		"\n"
		"section .data\n"
		"	instruction_number: EQU " << instrs.size() << "\n"
		"	instruction_offsets: dq ";
	
	outFile << "instr_0";
	for (int i = 1; i < instrs.size(); ++i) {
		outFile << ",instr_" << i;
	}
	
	outFile << "\n";
	outFile.close();
}

int main() {
	ifstream inFile;
	inFile.open("in.mx");
	vector<InstrToken> instrs = tokenize(inFile);
	generate(instrs);
}
