#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

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
// globals ---------------------------------
fs::path InputFileName = "";
fs::path OutputFileName = "";

bool FLAG_silent = false;
bool FLAG_run = false;
bool FLAG_keepTempFiles = false;
bool FLAG_strictErrors = false;
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
	OPt,

	OPand, // bitwise
	OPor,
	OPxor,

	OPno,
	OperationCount
};
static_assert(OperationCount == 7, "Exhaustive CharToOp definition");
map<char, OpNames> CharToOp = {
{'a', OPa}, // TODO: add +, - as suffixes
{'s', OPs},
{'t', OPt},

{'&', OPand},
{'|', OPor},
{'^', OPxor},
};
enum CondNames {
	Ceq,
	Cne,
	Clt,
	Cle,
	Cgt,
	Cge,

	Cno,
	ConditionCount
};
static_assert(ConditionCount == 7, "Exhaustive StrToCond definition");
map<string, CondNames> StrToCond = {
{"eq", Ceq},
{"ne", Cne},
{"lt", Clt},
{"le", Cle},
{"gt", Cgt},
{"ge", Cge},
};
enum InstrNames {
	Imov,
	Istr,
	Ild ,
	Ijmp,

	Ib,
	Il,

	Iswap,
	Ioutu,
	Ioutc,
	InstructionCount
};
static_assert(InstructionCount == 9, "Exhaustive StrToInstr definition");
map<string, InstrNames> StrToInstr {
{"mov", Imov},
{"str", Istr},
{"ld" , Ild },
{"jmp", Ijmp},

{"b", Ib},
{"l", Il},

{"swap", Iswap},
{"outu", Ioutu},
{"outc", Ioutc},
};
static_assert(InstructionCount == 9 && RegisterCount == 5, "Exhaustive InstrToModReg definition");
map<InstrNames, RegNames> InstrToModReg = {
	{Imov, Rh},
	{Istr, Rm},
	{Ild , Rr},
	{Ijmp, Rp},

	{Ib, Rno}, // conditionals cannot have a modifier
	{Il, Rno},

	{Iswap, Rno}, // specials have no destination
	{Ioutu, Rno},
	{Ioutc, Rno},
};
// structs -------------------------------
struct Loc {
	string file;
	int row;
	int col;

	Loc() {}
	Loc(string file, int row, int col) {
		this->file = file;
		this->row = row;
		this->col = col;
	}
	string toStr() {
		return file + ":" + to_string(row) + ":" + to_string(col);
	}
};
struct Suffix {
	// dest ?<operation>= <reg>/<imm>
	CondNames cond;
	OpNames modifier;
	RegNames reg;

	Suffix() {
		cond = Cno;
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
	bool hasCond() { return suffixes.cond != Cno; }
	bool hasMod()  { return suffixes.modifier != OPno; }
	bool hasReg()  { return suffixes.reg != Rno; }
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
	Label() { name=""; }
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
bool isntSpace(char c) {
	return !isspace(c);
}
vector<string> splitOnWhitespace(string &s) {
	vector<string> splits;
	int start = 0;
	for (int i = 0; i < s.size(); ++i) {
		if (isspace(s.at(i))) {
			string chunk = s.substr(start, i - start);
			if (chunk.size()) splits.push_back(chunk);
			start = i+1;
		}
	}
	if (start < s.size()) {
		string chunk = s.substr(start);
		splits.push_back(chunk);
	}
	return splits;
}
bool validIdentChar(char c) { return isalnum(c) || c == '_'; }
bool isValidIdentifier(string s) { // TODO: check for names too similar to an instruction
	// TODO: add reason for invalid identifier
	return s.size() > 0 && find_if_not(s.begin(), s.end(), validIdentChar) == s.end() && (isalpha(s.at(0)) || s.at(0) == '_');
}
fs::path pathFromMasfix(fs::path p) {
	fs::path out;
	bool found = false;
	for (fs::path part : p) {
		string s = part.string();
		if (found) out.append(s);
		if (s == "Masfix") {
			found = true;
		}
	}
	return found ? out : p;
}
// checks
#define unreachable() assert(("Unreachable", false));
#define continueOnFalse(cond) if (!(cond)) continue;
#define returnOnFalse(cond) if (!(cond)) return false;
#define checkContinueOnFail(cond, message, obj) if(!check(cond, message, obj, false)) continue;
#define checkReturnOnFail(cond, message, obj) if(!check(cond, message, obj, false)) return false;

vector<string> errors;
void raiseErrors() {
	for (string s : errors) {
		cerr << s;
	}
	if (errors.size())
		exit(1);
}
void addError(string message, bool strict=true) {
	errors.push_back(message);
	if (strict || FLAG_strictErrors)
		raiseErrors();
}
bool check(bool cond, string message, Instr instr, bool strict=true) {
	if (!cond) {
		string err = instr.loc.toStr() + " ERROR: " + message + " '" + instr.toStr() + "'\n";
		addError(err, strict);
	}
	return cond;
}
bool check(bool cond, string message, Label label, bool strict=true) {
	if (!cond) {
		string err = label.loc.toStr() + " ERROR: " + message + " '" + label.toStr() + "'\n";
		addError(err, strict);
	}
	return cond;
}
bool check(bool cond, string message, Loc loc, bool strict=true) {
	if (!cond) {
		string err = loc.toStr() + " ERROR: " + message + "\n";
		addError(err, strict);
	}
	return cond;
}
bool check(bool cond, string message, bool strict=true) {
	if (!cond) {
		string err = "ERROR: " + message + "\n";
		addError(err, strict);
	}
	return cond;
}
// tokenization ----------------------------------
bool parseSuffixes(Instr& instr, string s, bool condExpected=false) {
	static_assert(RegisterCount == 5 && OperationCount == 7 && ConditionCount == 7, "Exhaustive parseSuffixes definition");
	if (condExpected) {
		checkReturnOnFail(s.size() >= 2, "Missing condition", instr);
		string cond = s.substr(0, 2);
		checkReturnOnFail(StrToCond.count(cond) == 1, "Unknown condition", instr);
		instr.suffixes.cond = StrToCond[cond];
		s = s.substr(2);
	}
	checkReturnOnFail(s.size() <= 2, "Max two letter suffixes are supported for now", instr);
	if (s.size() == 1) {
		char c = s.at(0);
		if (CharToReg.count(c) == 1) {
			instr.suffixes.reg = CharToReg[c];
		} else if (CharToOp.count(c) == 1) {
			instr.suffixes.modifier = CharToOp[c];
		} else {
			checkReturnOnFail(false, "Unknown suffix", instr);
		}
	} else if (s.size() == 2) {
		checkReturnOnFail(CharToOp.count(s.at(0)) == 1, "The first suffix must be an operation", instr);
		checkReturnOnFail(CharToReg.count(s.at(1)) == 1, "The second suffix must be a register", instr);
		instr.suffixes.modifier = CharToOp[s.at(0)];
		instr.suffixes.reg = CharToReg[s.at(1)];
	}
	return true;
}
bool parseInstrOpcode(Instr& instr) {
	static_assert(InstructionCount == 9, "Exhaustive parseInstrOpcode definition");
	string parsing = instr.fields[0];
	for (int checkedLen : {2, 1, 3, 4}) { // avoid parsing 'ld' as Il
		if (parsing.size() < checkedLen) continue;
		string substr = parsing.substr(0, checkedLen);
		if (StrToInstr.count(substr) == 1) {
			instr.instr = StrToInstr[substr];
			string suffix = parsing.substr(checkedLen);
			return parseSuffixes(instr, suffix, instr.instr == Ib || instr.instr == Il);
		}
	}
	checkReturnOnFail(false, "Unknown instruction", instr);
	return false;
}
bool parseInstrImmediate(Instr& instr, map<string, Label> &stringToLabel) {
	string s = instr.fields[1];
	if (stringToLabel.count(s) > 0) {
		instr.immediate = stringToLabel[s].addr;
		return true;
	}
	checkReturnOnFail(isdigit(s.at(0)), "Invalid instruction immediate", instr);
	instr.immediate = stoi(s);
	checkReturnOnFail(to_string(instr.immediate) == s, "Invalid instruction immediate", instr);
	checkReturnOnFail(0 <= instr.immediate && instr.immediate <= WORD_MAX_VAL, "Value of the immediate is out of bounds", instr);
	return true;
}
bool parseInstrFields(Instr& instr, map<string, Label> &stringToLabel) {
	returnOnFalse(parseInstrOpcode(instr));
	instr.hasImm = instr.fields.size() == 2;
	if (instr.hasImm) {
		returnOnFalse(parseInstrImmediate(instr, stringToLabel));
	}
	return true;
}
pair<bool, string> parseLabelName(vector<string>& splits, Loc loc) {
	if (splits[0].at(0) != ':') {
		return pair(false, "");
	}
	if (splits[0] == ":") {
		if (!check(splits.size() == 2, "Label name expected after ':'", loc, false)) return pair(true, "");
		return pair(true, splits[1]);
	}
	string second = splits.size() == 2 ? splits[1] : "";
	if (!check(splits.size() == 1, "Invalid label definition '" + splits[0] + " " + second + "'", loc, false)) return pair(true, "");
	if (!check(splits[0].size() > 1, "Label name expected after ':'", loc, false)) return pair(true, "");
	return pair(true, splits[0].substr(1));
}
// checks if the instr has correct combination of suffixes and immediates
bool checkValidity(Instr& instr) {
	static_assert(InstructionCount == 9 && sizeof(Suffix) == 3 * 4, "Exhaustive checkValidity definition"); // TODO add these everywhere
	if (instr.instr == InstructionCount) unreachable();
	if (instr.instr == Iswap) {
		checkReturnOnFail(!instr.hasImm && !instr.hasReg(), "Swap instruction has no arguments", instr);
		instr.suffixes.reg = Rm; // pretend that the register is m
	}
	checkReturnOnFail(instr.hasImm ^ instr.hasReg(), "Instrs must have only imm, or 1 reg for now", instr);
	if (InstrToModReg[instr.instr] == Rno) {
		checkReturnOnFail(!instr.hasMod(), "This instruction cannot have a modifier", instr);
	}
	// NOTE: conditions are checked in parseSuffixes
	return true;
}
vector<Instr> tokenize(ifstream& inFile, fs::path fileName) {
	string file = pathFromMasfix(fileName).string();
	string line;

	vector<Instr> instrs;
	map<string, Label> strToLabel = {{"begin", Label("begin", 0, Loc(file, 1, 1))}, {"end", Label("end", 0, Loc(file, 1, 1))}};

	for (int lineNum = 1; getline(inFile, line); ++lineNum) {
		line = line.substr(0, line.find(';'));
		int lineStartCol = 1 + distance(line.begin(), find_if(line.begin(), line.end(), isntSpace));
		if (line.size() >= lineStartCol) {
			vector<string> splits = splitOnWhitespace(line);
			Loc loc = Loc(file, lineNum, lineStartCol);
			checkContinueOnFail(splits.size() <= 2, "This line has too many fields '" + line + "'", loc);
			pair<bool, string> lab = parseLabelName(splits, loc);
			if (!lab.first) {
				Instr instr(loc, splits);
				instrs.push_back(instr);
			} else {
				string labelName = lab.second;
				continueOnFalse(labelName != "");
				Label label = Label(labelName, instrs.size(), loc);
				checkContinueOnFail(isValidIdentifier(label.name), "Invalid label name", label);
				checkContinueOnFail(strToLabel.count(label.name) == 0, "Label redefinition", label);
				strToLabel.insert(pair<string, Label>(label.name, label));
			}
		}
	}
	strToLabel["end"].addr = instrs.size();
	for (int i = 0; i < instrs.size(); ++i) {
		Instr instr = instrs[i];
		continueOnFalse(parseInstrFields(instr, strToLabel));
		continueOnFalse(checkValidity(instr));
		instrs[i] = instr;
	}
	raiseErrors();
	return instrs;
}
// assembly generation ------------------------------------------
void genRegisterFetch(ofstream& outFile, RegNames reg, int instrNum, bool toSecond=true) {
	static_assert(RegisterCount == 5, "Exhaustive genRegisterFetch definition");
	string regName = toSecond ? "rcx" : "rbx";
	string shortReg = toSecond ? "cx" : "bx";
	if (reg == Rh) {
		outFile << "	mov " << regName <<", r14\n";
	} else if (reg == Rm) {
		outFile << "	xor " << regName << ", " << regName << "\n"
			"	mov " << shortReg << ", [2*r14+r13]\n";
	} else if (reg == Rr) {
		outFile << "	mov " << regName <<", r15\n";
	} else if (reg == Rp) {
		outFile << "	mov " << regName << ", " << instrNum << "\n";
	} else {
		unreachable();
	}
}
void genOperation(ofstream& outFile, OpNames op) {
	static_assert(OperationCount == 7, "Exhaustive genOperation definition");
	if (op == OPa) {
		outFile << "	add cx, bx\n";
	} else if (op == OPs) {
		outFile << "	sub bx, cx\n"
			"	mov rcx, rbx\n";
	} else if (op == OPt) {
		outFile << "	mov rax, rbx\n"
			"	mul rcx\n"
			"	mov cx, ax\n";
	} else if (op == OPand) {
		outFile << "	and rcx, rbx\n";
	} else if (op == OPor) {
		outFile << "	or rcx, rbx\n";
	} else if (op == OPxor) {
		outFile << "	xor rcx, rbx\n";
	} else {
		unreachable();
	}
}
static_assert(ConditionCount == 7, "Exhaustive _jmpInstr definition");
map<CondNames, string> _jmpInstr {
	{Ceq, "jne"},
	{Cne, "je"},
	{Clt, "jns"},
	{Cle, "jg"},
	{Cgt, "jle"},
	{Cge, "js"},
};
static_assert(ConditionCount == 7, "Exhaustive _condLoadInstr definition");
map<CondNames, string> _condLoadInstr {
	{Ceq, "cmove"},
	{Cne, "cmovne"},
	{Clt, "cmovs"},
	{Cle, "cmovle"},
	{Cgt, "cmovlg"},
	{Cge, "cmovns"},
};
void genCond(ofstream& outFile, InstrNames instr, CondNames cond, int instrNum) {
	static_assert(ConditionCount == 7, "Exhaustive genCond definition"); // TODO test all values with l<cond>
	if (instr == Ib) {
		outFile << "	cmp r15w, 0\n"
		"	" << _jmpInstr[cond] << " instr_" << instrNum + 1 << "\n";
	} else if (instr == Il) {
		outFile << "	mov rbx, r15\n"
			"	mov r15, 0\n"
			"	cmp bx, cx\n"
			"	" << _condLoadInstr[cond] << " r15, 1\n";
	} else {
		unreachable();
	}
}
void genInstrBody(ofstream& outFile, InstrNames instr, int instrNum) {
	static_assert(InstructionCount == 9, "Exhaustive genInstrBody definition");
	if (instr == Imov) {
		outFile << "	mov r14, rcx\n";
	} else if (instr == Istr) {
		outFile << "	mov [2*r14+r13], cx\n";
	} else if (instr == Ild) {
		outFile << "	mov r15, rcx\n";
	} else if (instr == Ijmp || instr == Ib) {
		outFile <<
		"	mov rsi, " << instrNum << "\n"
		"	cmp rcx, instruction_count\n"
		"	ja jmp_error\n"
		"	mov rbx, instruction_offsets\n"
		"	jmp [rbx+8*rcx]\n";
	} else if (instr == Il) { // handled in genCond
	} else if (instr == Iswap) { // m in second
		outFile << "	mov [2*r14+r13], r15w\n"
			"	mov r15, rcx\n";
	} else if (instr == Ioutu) {
		outFile << "	mov rax, rcx\n"
			"	call print_unsigned\n";
	} else if (instr == Ioutc) {
		outFile <<
			"	mov rdx, stdout_buff\n"
			"	mov [rdx], cl\n"
			"	mov r8, 1\n"
			"	call stdout_write\n";
	} else {
		unreachable();
	}
}
void genAssembly(ofstream& outFile, Instr instr, int instrNum) {	
	// head pos - r14, internal r reg - r15
	// intermediate values - first - rbx, second - rcx
	// addr of cells[0] - r13
	// TODO specify responsibilities for clamping and register preserving
	if (instr.hasImm) {
		outFile << "	mov rcx, " << instr.immediate << '\n';
	} else {
		genRegisterFetch(outFile, instr.suffixes.reg, instrNum);
	}
	if (instr.hasMod()) {
		genRegisterFetch(outFile, InstrToModReg[instr.instr], instrNum, false);
		genOperation(outFile, instr.suffixes.modifier);
	}
	if (instr.hasCond()) {
		genCond(outFile, instr.instr, instr.suffixes.cond, instrNum);
	}
	genInstrBody(outFile, instr.instr, instrNum);
}

void generate(ofstream& outFile, vector<Instr>& instrs) {
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
		"	call stderr_write\n"
		"	pop rax ; print instr number\n"
		"	call print_unsigned_err\n"
		"	pop rdx ; error message\n"
		"	pop r8\n"
		"	call stderr_write\n"
		"	pop rax ; errorneous value\n"
		"	call print_unsigned_err\n"
		"	mov rdx, stdout_buff\n"
		"	mov BYTE [rdx], 10 ; '\\n'\n"
		"	mov r8, 1\n"
		"	call stderr_write\n"
		"	mov rax, 1 ; exit(1)\n"
		"	call exit\n"
		"\n"
		"get_std_fds: ; prepares all std fds, regs unsafe!\n"
		"	mov rcx, -10 ; stdin fd\n"
		"	sub rsp, 32 ; call with shadow space\n"
		"	call GetStdHandle@4\n"
		"	add rsp, 32\n"
		"	mov rcx, stdin_fd\n"
		"	mov [rcx], rax\n"
		"	mov rcx, -11 ; stdout fd\n"
		"	sub rsp, 32\n"
		"	call GetStdHandle@4\n"
		"	add rsp, 32\n"
		"	mov rcx, stdout_fd\n"
		"	mov [rcx], rax\n"
		"	mov rcx, -12 ; stderr fd\n"
		"	sub rsp, 32\n"
		"	call GetStdHandle@4\n"
		"	add rsp, 32\n"
		"	mov rcx, stderr_fd\n"
		"	mov [rcx], rax\n"
		"	ret\n"
		"write_file: ; rcx - fd, rdx - buff, r8 - number of bytes -> rax - number written\n"
		"	push 0 ; number of bytes written var\n"
		"	mov r9, rsp ; ptr to that var\n"
		"	push 0 ; OVERLAPPED struct null ptr\n"
		"	sub rsp, 32 ; call with shadow space\n"
		"	call WriteFile@20\n"
		"	add rsp, 32+8 ; get rid of the OVERLAPPED\n"
		"	pop rax\n"
		"	ret\n"
		"stdout_write: ; rdx - buff, r8 - number of bytes -> rax - number written\n"
		"	mov rcx, stdout_fd\n"
		"	mov rcx, [rcx] ; stdout fd\n"
		"	call write_file\n"
		"	ret\n"
		"stderr_write: ; rdx - buff, r8 - number of bytes -> rax - number written\n"
		"	mov rcx, stderr_fd\n"
		"	mov rcx, [rcx] ; stderr fd\n"
		"	call write_file\n"
		"	ret\n"
		"utos: ; n - rax -> r8 char count, r9 - char* str\n"
		"	xor r8, r8 ; char count\n"
		"	mov r9, stdout_buff+" << STDOUT_BUFF_SIZE-1 << " ; curr buff pos\n"
		"	mov r10, 10 ; base\n"
		"utos_loop:\n"
		"	xor rdx, rdx\n"
		"	div r10\n"
		"	add rdx, '0'\n"
		"\n"
		"	inc r8\n"
		"	dec r9\n"
		"	mov [r9], dl\n"
		"\n"
		"	cmp rax, 0\n"
		"	jne utos_loop\n"
		"	ret\n"
		"print_unsigned: ; rax - n -> rax - num written\n"
		"	call utos\n"
		"	mov rdx, r9 ; str\n"
		"	call stdout_write\n"
		"	ret\n"
		"print_unsigned_err: ; rax - n -> rax - num written\n"
		"	call utos\n"
		"	mov rcx, stderr_fd\n"
		"	mov rcx, [rcx]\n"
		"	mov rdx, r9 ; str\n"
		"	call write_file\n"
		"	ret\n"
		"\n"
		"global _start\n"
		"_start:\n"
		"	; initialization\n"
		"	call get_std_fds\n"
		"	mov r13, cells\n"
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
		"	stdin_fd: resq 1\n"
		"	stdout_fd: resq 1\n"
		"	stderr_fd: resq 1\n"
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
	
	check(instrs.size() <= WORD_MAX_VAL+1, "The instruction count " + to_string(instrs.size()) + " exceeds WORD_MAX_VAL=" + to_string(WORD_MAX_VAL));
	outFile << "instr_0";
	for (int i = 1; i <= instrs.size(); ++i) {
		outFile << ",instr_" << i;
	}
	
	outFile << "\n";
	outFile.close();
}
// IO ---------------------------------------
void printUsage() {
	cout << "usage: Masfix [flags] <file-name>\n"
			"	flags:\n"
			"		-s / --silent     - supresses all unnecessary stdout messages\n"
			"		-r / --run        - run the executable after compilation\n"
			"		--keep-files      - keep the temporary compilation files\n" // TODO rather --keep-asm
			"		--strict-errors   - disables many errors\n";
}
void checkUsage(bool cond, string message) {
	if (!cond) {
		cerr << "ERROR: " << message << "\n\n";
		printUsage();
		exit(1);
	}
}
vector<string> getLineArgs(int argc, char *argv[]) {
	vector<string> args;
	for (int i = 0; i < argc; ++i) {
		args.push_back(argv[i]);
	}
	return args;
}
void genFileNames(string arg) {
	InputFileName = fs::absolute(arg);
	OutputFileName = InputFileName;
	OutputFileName.replace_extension("asm");
}
void processLineArgs(int argc, char *argv[]) {
	vector<string> args = getLineArgs(argc, argv);
	checkUsage(args.size() >= 2, "Insufficent number of command line args");
	vector<string> flags = vector<string>(++args.begin(), --args.end());
	for (string s : flags) {
		if (s == "-s" || s == "--silent") {
			FLAG_silent = true;
		} else if (s == "-r" || s == "--run") {
			FLAG_run = true;
		} else if (s == "--keep-files") {
			FLAG_keepTempFiles = true;
		} else if (s == "--strict-errors") {
			FLAG_strictErrors = true;
		} else {
			checkUsage(false, "Unknown command line arg '" + s + "'");
		}
	}
	genFileNames(args[args.size()-1]);
}
ifstream getInputFile() {
	ifstream inFile;
	check(fs::exists(InputFileName), "The input file '" + InputFileName.string() + "' couldn't be found");
	inFile.open(InputFileName);
	check(inFile.good(), "The input file '" + InputFileName.string() + "' couldn't be opened");
	return inFile;
}
ofstream getOutputFile() {
	ofstream outFile;
	outFile.open(OutputFileName);
	check(outFile.good(), "The output file '" + OutputFileName.string() + "' couldn't be opened");
	return outFile;
}
void runCmdEchoed(string command) {
	if (!FLAG_silent) {
		cout << "[CMD] " << command << '\n';
	}
	int returnCode = system(command.c_str());
	if (returnCode) {
		exit(returnCode);
	}
}
void removeFile(fs::path file) {
	bool ret = remove(file.string().c_str());
	if (ret) {
		cerr << "WARNING: error on removing the file '" << file.filename() << "'\n";
	}
}
void compileAndRun(fs::path asmPath) {
	fs::path objectPath = asmPath;
	objectPath.replace_extension("obj");
	fs::path exePath = asmPath;
	exePath.replace_extension("exe");

	runCmdEchoed("nasm -fwin64 " + asmPath.string());
	runCmdEchoed("ld C:\\Windows\\System32\\kernel32.dll -e _start -o " + exePath.string() + " " + objectPath.string());
	if (!FLAG_keepTempFiles) {
		removeFile(asmPath);
		removeFile(objectPath);
	}
	if (FLAG_run) {
		runCmdEchoed(exePath.string());
	}
}
int main(int argc, char *argv[]) {
	processLineArgs(argc, argv);
	ifstream inFile = getInputFile();
	vector<Instr> instrs = tokenize(inFile, InputFileName);
	inFile.close();
	ofstream outFile = getOutputFile();
	generate(outFile, instrs);
	outFile.close();
	compileAndRun(OutputFileName);
}
