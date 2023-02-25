#include <iostream>
#include <fstream>

#include <string>
#include <map>
#include <vector>

#include <algorithm>
#include <math.h>
#include <assert.h>
#include <functional> 
#include <cctype>
#include <locale>
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

#define unreachable() assert(("Unreachable", false));
void check(bool cond, string message) {
	if (!cond) {
		cout << "ERROR: " << message << '\n';
		exit(1);
	}
}
// enums --------------------------------
enum InstrNames {
	Imov,
	Istr,
	Ild ,
	Ijmp,

	Ioutu,
	Ioutc,
	InstructionCount
};
static_assert(InstructionCount == 6, "Exhaustive InstrToStr definition");
map<InstrNames, string> InstrToStr {
{Imov, "mov"},
{Istr, "str"},
{Ild , "ld" },
{Ijmp, "jmp"},

{Ioutu, "outu"},
{Ioutc, "outc"},
};

enum SuffixNames {
	Sm,
	Sr,
	SuffixCount
};
static_assert(SuffixCount == 2, "Exhaustive SuffixToChar definition");
map<SuffixNames, char> SuffixToChar {
{Sm, 'm'},
{Sr, 'r'},
};
// structs -------------------------------
struct Instr {
	InstrNames instr;
	vector<SuffixNames> suffixes;
	bool hasImm;
	int immediate;
	
	Instr() {}
	Instr(InstrNames instr, bool hasImm, int immediate) {
		this->instr = instr;
		this->hasImm = hasImm;
		this->immediate = immediate;
	}
	string toStr() {
		string s = InstrToStr[instr];
		for (SuffixNames suff : suffixes) {
			s.push_back(SuffixToChar[suff]);
		}
		s.push_back(' ');
		if (hasImm) {
			s.append(to_string(immediate));
		}
		return s;
	}
};
// tokenization ----------------------------------
vector<SuffixNames> parseSuffixes(string s) {
	static_assert(SuffixCount == 2, "Exhaustive parseSuffixes definition");
	check(s.size() <= 1, "Only one letter suffixes are supported for now: '" + s + "'");
	vector<SuffixNames> out;
	if (s.size() == 0) return out;
	char suff = s.at(0);

	map<char, SuffixNames> m {{'m', Sm}, {'r', Sr}};
	check(m.count(suff) == 1, "Unknown suffix '" + s + "'\n");
	out.push_back(m[suff]);
	return out;
}
Instr parseInstrOpcode(string parsing) {
	static_assert(InstructionCount == 6, "Exhaustive parseInstrOpcode definition");
	Instr instr;
	for (pair<InstrNames, string> p : InstrToStr) {
		string substr = parsing.substr(0, p.second.size());
		if (substr == p.second) {
			instr.instr = p.first;
			string suffix = parsing.substr(p.second.size());
			instr.suffixes = parseSuffixes(suffix);
			return instr;
		}
	}
	check(false, "Unknown instruction: '" + parsing + "'");
	return instr; // so the compiler doesn't yell at us
}
int parseInstrImmediate(string s) {
	check(isdigit(s.at(0)), "Invalid instruction immediate '" + s + "'");
	int imm = stoi(s);
	check(to_string(imm) == s, "Invalid instruction immediate '" + s + "'");
	check(0 <= imm && imm < (int)exp2(16), "Value of the immediate '" + to_string(imm) + "' is out of bounds");
	return imm;
}
Instr parseInstrFields(vector<string> &fields) {
	Instr instr = parseInstrOpcode(fields[0]);
	instr.hasImm = fields.size() == 2;
	if (instr.hasImm) {
		instr.immediate = parseInstrImmediate(fields[1]);
	}
	return instr;
}
vector<Instr> tokenize(ifstream& inFile) {
	string line;
	vector<Instr> instrs;
	
	while (getline(inFile, line)) {
		line = line.substr(0, line.find(';'));
		line = trim(line);
		if (line.size() > 0) {
			vector<string> splits = splitOnWhitespace(line);
			check(splits.size() <= 2, "Instruction has too many fields '" + line + "'");
			
			Instr instr = parseInstrFields(splits);
			instrs.push_back(instr);
		}
	}
	return instrs;
}
// assembly generation ------------------------------------------
void genAssembly(ofstream& outFile, Instr instr) {	
	// head pos - rbx, internal r reg - rcx
	static_assert(InstructionCount == 6, "Exhaustive genAssembly definition");
	static_assert(SuffixCount == 2, "Exhaustive genAssembly definition");
	// TODO: instrs using immediate should check if the instr hasImm
	if (instr.instr == Imov) {
		outFile << "	mov rbx, " << instr.immediate << '\n';
	} else if (instr.instr == Istr) {
		outFile << "	mov WORD [2*rbx+cells], " << instr.immediate << '\n';
	} else if (instr.instr == Ild) {
		outFile << "	mov rcx, " << instr.immediate << '\n';
	} else if (instr.instr == Ioutu) {
		if (instr.suffixes[0] == Sm) {
			outFile << "	mov ax, [2*rbx+cells]\n"
			"	call print_unsigned\n";
		} else if (instr.suffixes[0] == Sr) {
			outFile << "	mov ax, cx\n"
						"	call print_unsigned\n";
		} else {
			unreachable();
		}		
	} else if (instr.instr == Ioutc) {
		outFile << // TODO: outc should use a BYTE inst of WORD, and should check the value to be < 256
			"	mov [stdout_buff], WORD " << instr.immediate << "\n"
			"	mov rdx, stdout_buff\n"
			"	mov r8, 1\n"
			"	call stdout_write\n";
	} else if (instr.instr == Ijmp) { // TODO: add guards to jmp so we don't jump somewhere which DNE
		outFile << "	jmp [instruction_offsets+8*" << instr.immediate << "]\n";
	} else {
		unreachable();
	}
}

void generate(vector<Instr>& instrs, string outFileName="out.asm") {
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
	
	Instr instr;
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
	vector<Instr> instrs = tokenize(inFile);
	generate(instrs);
}
