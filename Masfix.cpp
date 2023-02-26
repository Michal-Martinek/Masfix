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

// constants -------------------------------
#define STDOUT_BUFF_SIZE 256
#define WORD_MAX_VAL 65535
#define CELLS WORD_MAX_VAL+1

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
		if (hasImm) {
			s.push_back(' ');
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
	check(m.count(suff) == 1, "Unknown suffix '" + s + "'\n"); // TODO: print whole instr
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
int parseInstrImmediate(string s) { // TODO: on ERROR print whole instr
	check(isdigit(s.at(0)), "Invalid instruction immediate '" + s + "'");
	int imm = stoi(s);
	check(to_string(imm) == s, "Invalid instruction immediate '" + s + "'");
	check(0 <= imm && imm <= WORD_MAX_VAL, "Value of the immediate '" + to_string(imm) + "' is out of bounds");
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
void genArgumentFetch(ofstream& outFile, Instr instr) {
	static_assert(SuffixCount == 2, "Exhaustive genArgumentFetch definition");
	check(instr.hasImm ^ instr.suffixes.size() == 1, "Instrs must have only imm, or 1 reg for now: '" + instr.toStr() + "'\n");
	if (instr.hasImm) {
		outFile << "	mov rax, " << instr.immediate << '\n';
		return;
	}
	SuffixNames suff = instr.suffixes[0];
	if (suff == Sm) {
		outFile << "	xor rax, rax\n"
			"	mov ax, [2*rbx+cells]\n";
	} else if (suff == Sr) {
		outFile << "	xor rax, rax\n"
			"	mov ax, cx\n";
	} else {
		unreachable();
	}
}
void genInstrBody(ofstream& outFile, Instr instr, int instrNum) {
	static_assert(InstructionCount == 6, "Exhaustive genInstrBody definition");
	if (instr.instr == Imov) {
		outFile << "	mov rbx, rax\n";
	} else if (instr.instr == Istr) {
		outFile << "	mov [2*rbx+cells], ax\n";
	} else if (instr.instr == Ild) {
		outFile << "	mov rcx, rax\n";
	} else if (instr.instr == Ioutu) {
		outFile << "	call print_unsigned\n";
	} else if (instr.instr == Ioutc) {
		outFile <<
			"	mov [stdout_buff], al\n"
			"	mov rdx, stdout_buff\n"
			"	mov r8, 1\n"
			"	call stdout_write\n";
	} else if (instr.instr == Ijmp) {
		outFile <<
		"	mov rsi, " << instrNum << "\n"
		"	cmp rax, instruction_count\n"
		"	ja jmp_error\n"
		"	jmp [instruction_offsets+8*rax]\n";
	} else {
		unreachable();
	}
}
void genAssembly(ofstream& outFile, Instr instr, int instrNum) {	
	// head pos - rbx, internal r reg - rcx
	genArgumentFetch(outFile, instr); // loads instr argument in ax, responsible for 16-bit clamping
	genInstrBody(outFile, instr, instrNum); // executes the instr body, given instr val in ax, responsible for clamping the destination
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
		"exit: ; exits the program with code in rax\n"
		"	mov rcx, rax\n"
		"	sub rsp, 32\n"
		"	call ExitProcess@4\n"
		"	hlt\n"
		"error: ; prints ERROR template, instr number: rsi, message: rdx, r8, errorneous value: rax, exit(1)\n"
		"	push rax\n"
		"	push r8\n"
		"	push rdx\n"
		"	push rsi\n"
		"	mov rdx, ERROR_template\n"
		"	mov r8, ERROR_template_len\n"
		"	call stdout_write\n"
		"	pop rax ; print instr number\n"
		"	call print_unsigned\n"
		"	pop rdx ; error message\n"
		"	pop r8\n"
		"	call stdout_write\n"
		"	pop rax ; errorneous value\n"
		"	call print_unsigned\n"
		"	mov BYTE [stdout_buff], 10 ; '\\n'\n"
		"	mov rdx, stdout_buff\n"
		"	mov r8, 1\n"
		"	call stdout_write\n"
		"	mov rax, 1 ; exit(1)\n"
		"	call exit\n"
		"\n"
		"get_std_fds: ; prepares all std fds, regs unsafe!\n"
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
		"	mov r9, stdout_buff+" << STDOUT_BUFF_SIZE-1 << " ; curr buff pos\n"
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
		genAssembly(outFile, instr, i);
	} 
	outFile <<
		"instr_"<< instrs.size() << ":\n"
		"	jmp end\n"
		"\n"
		"; runtime errors, expect instr number in rsi, errorneous value in rax\n"
		"jmp_error:\n"
		"	mov rdx, jmp_error_message\n"
		"	mov r8, jmp_error_message_len\n"
		"	call error\n"
		"\n"
		"end:\n"
		"	; exit(0)\n"
		"	mov rax, 0\n"
		"	call exit\n"
		"\n"
		"section .bss\n"
		"	cells: resw " << CELLS << "\n"
		"	stdout_fd: resq 1\n"
		"	stdout_buff: resb " << STDOUT_BUFF_SIZE << "\n"
		"\n"
		"section .data\n"
		"	; error messages\n"
		"	ERROR_template: db 10,\"ERROR: instr_\"\n"
		"	ERROR_template_len: EQU $-ERROR_template\n"
		"	jmp_error_message: db \": jmp destination out of bounds: \"\n"
		"	jmp_error_message_len: EQU $-jmp_error_message\n"
		"\n"
		"	; instruction addresses\n"
		"	instruction_count: EQU " << instrs.size() << "\n"
		"	instruction_offsets: dq ";
	
	check(instrs.size() < CELLS, "The instruction count " + to_string(instrs.size()) + "exceeds maximum word value");
	outFile << "instr_0";
	for (int i = 1; i <= instrs.size(); ++i) {
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
