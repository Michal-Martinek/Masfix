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
string deleteWhitespace(string s) {
	for (int i = 0; i < s.size();) {
		if (std::isspace(s.at(i)))
			s.erase(i, 1);
		else i++;
	}
	return s;
}
/// @brief splits the string into fields on any non-alphanumeric character except '_', ignores whitespace
/// @param s string to split
/// @return vector of fields
vector<string> splitDeleteWhitespace(string &s) {
	vector<string> splits;
	int start = 0;
	for (int i = 0; i < s.size(); ++i) {
		// TODO: make '-' between two letters not split
		// NOTE: notice the ambiguity
		// :first
		// :second
		// :first-second ; OK - labels are just identifier unless explicit operation is in them ('()')
		// jmp first-second ; BAD obvious here
		if (!(isalnum(s.at(i)) || s.at(i) == '_')) {
			string chunk = s.substr(start, i - start);
			if (chunk.size()) splits.push_back(chunk);
			chunk = s.substr(i, 1); // this char stands alone, split it
			chunk = deleteWhitespace(chunk);
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
bool validIdentChar(char c) { return isalnum(c) || c == '_'; }
bool isValidIdentifier(string s) { // TODO: check for names too similar to an instruction
	// TODO: add reason for invalid identifier
	return s.size() > 0 && find_if_not(s.begin(), s.end(), validIdentChar) == s.end() && (isalpha(s.at(0)) || s.at(0) == '_');
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
static_assert(InstructionCount == 6, "Exhaustive StrToInstr definition");
map<string, InstrNames> StrToInstr {
{"mov", Imov},
{"str", Istr},
{"ld" , Ild },
{"jmp", Ijmp},

{"outu", Ioutu},
{"outc", Ioutc},
};

enum SuffixNames {
	Rm, // registers
	Rr,

	Oa, // operations
	Os,

	Sno, // no reg or op
	SuffixCount
};
static_assert(SuffixCount == 5, "Exhaustive CharToSuffix definition");
map<char, SuffixNames> CharToSuffix {
{'m', Rm},
{'r', Rr},

{'a', Oa}, // TODO: add +, - as suffixes
{'s', Os},
};
// structs -------------------------------
struct Suffix {
	// dest ?<operation>= <reg>/<imm>
	SuffixNames modifier;
	SuffixNames reg;

	Suffix() {
		modifier = Sno;
		reg = Sno;
	}
};
struct Instr {
	InstrNames instr;
	Suffix suffixes;
	bool hasImm;
	int immediate;

	// TODO: add loc, original string
	
	Instr() {
		this->instr = InstructionCount; // invalid
		this->suffixes = Suffix();
		this->hasImm = false;
	}
	bool hasMod() { return suffixes.modifier != Sno; }
	bool hasReg() { return suffixes.reg != Sno; }
	string toStr() {
		return "no string";
	}
};
struct Label {
	string name;
	int addr; // TODO: add loc
	Label() {}
	Label(string name, int addr) {
		this->name = name;
		this->addr = addr;
	}
};
// tokenization ----------------------------------
void parseSuffixes(Instr& instr, string s) { // TODO: print whole instr
	static_assert(SuffixCount == 5, "Exhaustive parseSuffixes definition");
	check(s.size() <= 2, "Max two letter suffixes are supported for now: '" + s + "'");
	if (s.size() == 1) {
		SuffixNames suff = CharToSuffix[s.at(0)];
		if (suff == Rm || suff == Rr) 
			instr.suffixes.reg = suff;
		else
			instr.suffixes.modifier = suff;
	} else if (s.size() == 2) {
		SuffixNames suff1 = CharToSuffix[s.at(0)], suff2 = CharToSuffix[s.at(1)];
		check(suff1 == Oa || suff1 == Os, "The first suffix must be an operation '" + s + "'\n");
		check(suff2 == Rm || suff2 == Rr, "The second suffix must be a register '" + s + "'\n");
		instr.suffixes.modifier = suff1;
		instr.suffixes.reg = suff2;
	}
}
void parseInstrOpcode(Instr& instr, string parsing) {
	static_assert(InstructionCount == 6, "Exhaustive parseInstrOpcode definition");
	for (int checkedLen = 2; parsing.size() >= checkedLen && checkedLen <= 4; ++checkedLen) {
		string substr = parsing.substr(0, checkedLen);
		if (StrToInstr.count(substr) == 1) {
			instr.instr = StrToInstr[substr];
			string suffix = parsing.substr(checkedLen);
			parseSuffixes(instr, suffix);
			return;
		}
	}
	check(false, "Unknown instruction: '" + parsing + "'");
}
int parseInstrImmediate(string s, map<string, Label> &stringToLabel) { // TODO: on ERROR print whole instr
	if (stringToLabel.count(s) > 0) {
		return stringToLabel[s].addr;
	}
	check(isdigit(s.at(0)), "Invalid instruction immediate '" + s + "'");
	int imm = stoi(s);
	check(to_string(imm) == s, "Invalid instruction immediate '" + s + "'");
	check(0 <= imm && imm <= WORD_MAX_VAL, "Value of the immediate '" + to_string(imm) + "' is out of bounds");
	return imm;
}
Instr parseInstrFields(Instr& instr, vector<string> &fields, map<string, Label> &stringToLabel) {
	parseInstrOpcode(instr, fields[0]);
	instr.hasImm = fields.size() == 2;
	if (instr.hasImm) {
		instr.immediate = parseInstrImmediate(fields[1], stringToLabel);
	}
	return instr;
}
vector<Instr> tokenize(ifstream& inFile) {
	string line;
	vector<vector<string>> instrsFields;

	vector<Instr> instrs;
	map<string, Label> strToLabel = {{"begin", Label("begin", 0)}, {"end", Label("end", 0)}};

	while (getline(inFile, line)) {
		line = line.substr(0, line.find(';'));
		line = trim(line);
		if (line.size() > 0) {
			vector<string> splits = splitDeleteWhitespace(line);
			check(splits.size() <= 2, "This line has too many fields '" + line + "'");
			if (splits[0] == ":") {
				check(splits.size() == 2, "Label name expected '" + line + "'");
				string ident = splits[1];
				check(isValidIdentifier(ident), "Invalid label name '" + line + "'");
				check(strToLabel.count(ident) == 0, "Label redefinition '" + line + "'");
				strToLabel.insert(pair<string, Label>(ident, Label(ident, instrsFields.size())));
			} else {
				instrsFields.push_back(splits);
			}
		}
	}
	strToLabel["end"].addr = instrsFields.size();
	for (vector<string> splits : instrsFields) {
		Instr instr;
		parseInstrFields(instr, splits, strToLabel);
		instrs.push_back(instr);
	}
	return instrs;
}
// assembly generation ------------------------------------------
void checkValidity(Instr instr) {
	static_assert(sizeof(Suffix) == 2 * 4, "Exhaustive checkValidity definition"); // TODO add these everywhere
	if (instr.instr == InstructionCount) unreachable();
	check(instr.hasImm ^ instr.hasReg(), "Instrs must have only imm, or 1 reg for now: '" + instr.toStr() + "'\n");
}
void genArgumentFetch(ofstream& outFile, Instr instr) {
	static_assert(SuffixCount == 5, "Exhaustive genArgumentFetch definition");
	if (instr.hasImm) {
		outFile << "	mov rax, " << instr.immediate << '\n';
		return;
	}
	SuffixNames suff = instr.suffixes.reg;
	if (suff == Rm) {
		outFile << "	xor rax, rax\n"
			"	mov ax, [2*rbx+cells]\n";
	} else if (suff == Rr) {
		outFile << "	mov rax, rcx\n";
	} else {
		unreachable();
	}
}
void genDestFetch(ofstream& outFile, Instr instr, int instrNum) {
	static_assert(InstructionCount == 6, "Exhaustive genDestFetch definition");
	if (instr.instr == Imov) {
		outFile << "	mov rdx, rbx\n";
	} else if (instr.instr == Istr) {
		outFile << "xor rdx, rdx\n"
			"	mov dx, [2*rbx+cells]\n";
	} else if (instr.instr == Ild) {
		outFile << "	mov rdx, rcx\n";
	} else if (instr.instr == Ijmp) {
		outFile << "mov rdx, " << instrNum << "\n";
	} else {
		check(false, "This instruction can not have a modifier '" + instr.toStr() + "'\n");
	}
}
void genOperation(ofstream& outFile, SuffixNames op) {
	static_assert(SuffixCount == 5, "Exhaustive genOperation definition");
	if (op == Oa) {
		outFile << "	add ax, dx\n"; // TODO test
	}
	if (op == Os) {
		outFile << "	sub ax, dx\n";
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
	// head pos - bx, internal r reg - cx
	// intermeiate values - ax, first op arg - dx // TODO: rdx is not the best pick
	checkValidity(instr); // checks if the instr has correct combination of suffixes and immediates
	genArgumentFetch(outFile, instr); // loads instr argument in ax, responsible for 16-bit clamping
	if (instr.hasMod()) {
		genDestFetch(outFile, instr, instrNum); // loads instr destination in dx, responsible for 16-bit clamping
		genOperation(outFile, instr.suffixes.modifier); // generates operation body (dx op ax) -> ax, expects rax < 65536, responsible for 16-bit clamping
	}
	genInstrBody(outFile, instr, instrNum); // generates the instr body, given instr val in ax, responsible for clamping the destination
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
