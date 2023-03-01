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

// enums --------------------------------
enum RegNames {
	Rh,
	Rm,
	Rr,
	Rp,

	Rno,
	RegisterCount
};
static_assert(RegisterCount == 5, "Exhaustive CharToReg definition");
map<char, RegNames> CharToReg = {
{'h', Rh},
{'m', Rm},
{'r', Rr},
{'p', Rp},
};
enum OpNames {
	OPa,
	OPs,

	OPno,
	OperationCount
};
static_assert(OperationCount == 3, "Exhaustive CharToOp definition");
map<char, OpNames> CharToOp = {
{'a', OPa}, // TODO: add +, - as suffixes
{'s', OPs},
};
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
static_assert(InstructionCount == 6 && RegisterCount == 5, "Exhaustive InstrToDestReg definition");
map<InstrNames, RegNames> InstrToDestReg = {
	{Imov, Rh},
	{Istr, Rm},
	{Ild , Rr},
	{Ijmp, Rp},

	{Ioutu, Rno}, // specials have no destination
	{Ioutc, Rno},
};
// structs -------------------------------
struct Loc {
	int row;
	int col;
	string file = "in.mx";

	Loc() {}
	Loc(int row, int col) {
		this->row = row;
		this->col = col;
	}
	string toStr() {
		return file + ":" + to_string(row) + ":" + to_string(col);
	}
};
struct Suffix {
	// dest ?<operation>= <reg>/<imm>
	OpNames modifier;
	RegNames reg;

	Suffix() {
		modifier = OPno;
		reg = Rno;
	}
};
struct Instr {
	InstrNames instr;
	Suffix suffixes;
	bool hasImm;
	int immediate;

	Loc loc;
	vector<string> fields; // TODO loc for each field, suffixes
	
	Instr() {}
	Instr(Loc loc, vector<string> fields) {
		this->instr = InstructionCount; // invalid
		this->suffixes = Suffix();
		this->hasImm = false;
		this->loc = loc;
		this->fields = fields;
	}
	bool hasMod() { return suffixes.modifier != OPno; }
	bool hasReg() { return suffixes.reg != Rno; }
	string toStr() {
		if (fields.size() == 1) return fields[0];
		string s = fields[0];
		s.push_back(' ');
		s.append(fields[1]);
		return s;
	}
};
struct Label {
	string name;
	int addr;
	Loc loc;
	Label() {}
	Label(string name, int addr, Loc loc) {
		this->name = name;
		this->addr = addr;
		this->loc = loc;
	}
	string toStr() {
		return ":" + name;
	}
};
// helpers ---------------------------------
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
void check(bool cond, string message, Instr instr) {
	if (!cond) {
		cout << instr.loc.toStr() << " ERROR: " << message << " '" << instr.toStr() << "'\n";
		exit(1);
	}
}
void check(bool cond, string message, Label label) {
	if (!cond) {
		cout << label.loc.toStr() << " ERROR: " << message << " '" << label.toStr() << "'\n";
		exit(1);
	}
}
void check(bool cond, string message, Loc loc) {
	if (!cond) {
		cout << loc.toStr() << " ERROR: " << message << "\n";
		exit(1);
	}
}
// tokenization ----------------------------------
void parseSuffixes(Instr& instr, string s) {
	static_assert(RegisterCount == 5 && OperationCount == 3, "Exhaustive parseSuffixes definition");
	check(s.size() <= 2, "Max two letter suffixes are supported for now", instr);
	if (s.size() == 1) {
		char c = s.at(0);
		if (CharToReg.count(c) == 1) {
			instr.suffixes.reg = CharToReg[c];
		} else if (CharToOp.count(c) == 1) {
			instr.suffixes.modifier = CharToOp[c];
		} else {
			check(false, "Unknown suffix", instr);
		}
	} else if (s.size() == 2) {
		check(CharToOp.count(s.at(0)) == 1, "The first suffix must be an operation", instr);
		check(CharToReg.count(s.at(1)) == 1, "The second suffix must be a register", instr);
		instr.suffixes.modifier = CharToOp[s.at(0)];
		instr.suffixes.reg = CharToReg[s.at(1)];
	}
}
void parseInstrOpcode(Instr& instr) {
	static_assert(InstructionCount == 6, "Exhaustive parseInstrOpcode definition");
	string parsing = instr.fields[0];
	for (int checkedLen = 2; parsing.size() >= checkedLen && checkedLen <= 4; ++checkedLen) {
		string substr = parsing.substr(0, checkedLen);
		if (StrToInstr.count(substr) == 1) {
			instr.instr = StrToInstr[substr];
			string suffix = parsing.substr(checkedLen);
			parseSuffixes(instr, suffix);
			return;
		}
	}
	check(false, "Unknown instruction", instr);
}
void parseInstrImmediate(Instr& instr, map<string, Label> &stringToLabel) {
	string s = instr.fields[1];
	if (stringToLabel.count(s) > 0) {
		instr.immediate = stringToLabel[s].addr;
		return;
	}
	check(isdigit(s.at(0)), "Invalid instruction immediate", instr);
	instr.immediate = stoi(s);
	check(to_string(instr.immediate) == s, "Invalid instruction immediate", instr);
	check(0 <= instr.immediate && instr.immediate <= WORD_MAX_VAL, "Value of the immediate is out of bounds", instr);
}
void parseInstrFields(Instr& instr, map<string, Label> &stringToLabel) {
	parseInstrOpcode(instr);
	instr.hasImm = instr.fields.size() == 2;
	if (instr.hasImm) {
		parseInstrImmediate(instr, stringToLabel);
	}
}
vector<Instr> tokenize(ifstream& inFile) {
	string line;

	vector<Instr> instrs;
	map<string, Label> strToLabel = {{"begin", Label("begin", 0, Loc(1, 1))}, {"end", Label("end", 0, Loc(1, 1))}};

	for (int lineNum = 1; getline(inFile, line); ++lineNum) {
		line = line.substr(0, line.find(';'));
		int lineStartCol = 1 + distance(line.begin(), find_if(line.begin(), line.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
		if (line.size() >= lineStartCol) {
			vector<string> splits = splitDeleteWhitespace(line);
			Loc loc = Loc(lineNum, lineStartCol);
			check(splits.size() <= 2, "This line has too many fields '" + line + "'", loc);
			if (splits[0] == ":") {
				check(splits.size() == 2, "Label name expected after ':'", loc);
				Label label = Label(splits[1], instrs.size(), loc);
				check(isValidIdentifier(label.name), "Invalid label name", label);
				check(strToLabel.count(label.name) == 0, "Label redefinition", label);
				strToLabel.insert(pair<string, Label>(label.name, label));
			} else {
				Instr instr(loc, splits);
				instrs.push_back(instr);
			}
		}
	}
	strToLabel["end"].addr = instrs.size();
	for (int i = 0; i < instrs.size(); ++i) {
		Instr instr = instrs[i];
		parseInstrFields(instr, strToLabel);
		instrs[i] = instr;
	}
	return instrs;
}
// assembly generation ------------------------------------------
void checkValidity(Instr instr) {
	static_assert(InstructionCount == 6 && sizeof(Suffix) == 2 * 4, "Exhaustive checkValidity definition"); // TODO add these everywhere
	if (instr.instr == InstructionCount) unreachable();
	check(instr.hasImm ^ instr.hasReg(), "Instrs must have only imm, or 1 reg for now", instr);
	if (InstrToDestReg[instr.instr] == Rno) {
		check(!instr.hasMod(), "Special instructions cannot have a modifier", instr);
	}
}
void genRegisterFetch(ofstream& outFile, RegNames reg, int instrNum, bool toSecond=true) {
	static_assert(RegisterCount == 5, "Exhaustive genRegisterFetch definition");
	string regName = toSecond ? "rcx" : "rbx";
	string shortReg = toSecond ? "cx" : "bx";
	if (reg == Rh) {
		outFile << "	mov " << regName <<", r14\n";
	} else if (reg == Rm) {
		outFile << "	xor " << regName << ", " << regName << "\n"
			"	mov " << shortReg << ", [2*r14+cells]\n";
	} else if (reg == Rr) {
		outFile << "	mov " << regName <<", r15\n";
	} else if (reg == Rp) {
		outFile << "	mov " << regName << ", " << instrNum << "\n";
	} else {
		unreachable();
	}
}
void genOperation(ofstream& outFile, OpNames op) {
	static_assert(OperationCount == 3, "Exhaustive genOperation definition");
	if (op == OPa) {
		outFile << "	add rcx, rbx\n"; // TODO test
	} else if (op == OPs) {
		outFile << "	sub rbx, rcx\n"
			"	mov rcx, rbx\n";
	} else {
		unreachable();
	}
}
void genInstrBody(ofstream& outFile, InstrNames instr, int instrNum) {
	static_assert(InstructionCount == 6, "Exhaustive genInstrBody definition");
	if (instr == Imov) {
		outFile << "	mov r14, rcx\n";
	} else if (instr == Istr) {
		outFile << "	mov [2*r14+cells], cx\n";
	} else if (instr == Ild) {
		outFile << "	mov r15, rcx\n";
	} else if (instr == Ioutu) {
		outFile << "	mov rax, rcx\n"
			"	call print_unsigned\n";
	} else if (instr == Ioutc) {
		outFile <<
			"	mov [stdout_buff], cl\n"
			"	mov rdx, stdout_buff\n"
			"	mov r8, 1\n"
			"	call stdout_write\n";
	} else if (instr == Ijmp) {
		outFile <<
		"	mov rsi, " << instrNum << "\n"
		"	cmp rcx, instruction_count\n"
		"	ja jmp_error\n"
		"	jmp [instruction_offsets+8*rcx]\n";
	} else {
		unreachable();
	}
}
void genAssembly(ofstream& outFile, Instr instr, int instrNum) {	
	// head pos - r14, internal r reg - r15
	// intermediate values - first - rbx, second - rcx
	// TODO specify responsibilities for clamping and register preserving
	checkValidity(instr); // checks if the instr has correct combination of suffixes and immediates
	if (instr.hasImm) {
		outFile << "	mov rcx, " << instr.immediate << '\n';
	} else {
		genRegisterFetch(outFile, instr.suffixes.reg, instrNum);
	}
	if (instr.hasMod()) {
		genRegisterFetch(outFile, InstrToDestReg[instr.instr], instrNum, false);
		genOperation(outFile, instr.suffixes.modifier);
	}
	genInstrBody(outFile, instr.instr, instrNum);
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
		"error: ; prints ERROR template, instr number: rsi, message: rdx, r8, errorneous value: rcx, exit(1)\n"
		"	push rcx\n"
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
		"	xor r14, r14\n"
		"	xor r15, r15\n"
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
		"; runtime errors, expect instr number in rsi, errorneous value in rcx\n"
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
	
	check(instrs.size() <= WORD_MAX_VAL+1, "The instruction count " + to_string(instrs.size()) + " exceeds WORD_MAX_VAL=" + to_string(WORD_MAX_VAL), instrs[instrs.size()-1].loc);
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
