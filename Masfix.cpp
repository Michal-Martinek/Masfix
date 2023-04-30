#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#include <string>
#include <map>
#include <vector>
#include <list>
#include <stack>

#include <algorithm>
#include <math.h>
#include <assert.h>
#include <functional>
#include <cctype>
#include <locale>
using namespace std;

// constants -------------------------------
#define STDOUT_BUFF_SIZE 256
#define STDIN_BUFF_SIZE 256

#define WORD_MAX_VAL 65535
#define CELLS WORD_MAX_VAL+1
// globals ---------------------------------
fs::path InputFileName = "";
fs::path OutputFileName = "";

bool FLAG_silent = false;
bool FLAG_run = false;
bool FLAG_keepAsm = false;
bool FLAG_strictErrors = false;
// enums --------------------------------
enum TokenTypes {
	Tnumeric,
	Talpha,
	Tspecial,
	Tlist,

	TokenCount
};
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

	OPshl,
	OPshr,
	OPbit,

	OPno,
	OperationCount
};
static_assert(OperationCount == 10, "Exhaustive CharToOp definition");
map<char, OpNames> CharToOp = {
{'a', OPa}, // TODO: add +, - as suffixes
{'s', OPs},
{'t', OPt},

{'&', OPand},
{'|', OPor},
{'^', OPxor},

{'<', OPshl},
{'>', OPshr},
{'.', OPbit},
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
	Is,

	Iswap,
	Ioutu,
	Ioutc,
	Iinc,
	Iipc,
	Iinu,
	Iinl,
	InstructionCount
};
static_assert(InstructionCount == 14, "Exhaustive StrToInstr definition");
map<string, InstrNames> StrToInstr {
{"mov", Imov},
{"str", Istr},
{"ld" , Ild },
{"jmp", Ijmp},

{"b", Ib},
{"l", Il},
{"s", Is},

{"swap", Iswap},
{"outu", Ioutu},
{"outc", Ioutc},
{"inc", Iinc},
{"ipc", Iipc},
{"inu", Iinu},
{"inl", Iinl},
};
map<InstrNames, RegNames> InstrToModReg = {
	{Imov, Rh},
	{Istr, Rm},
	{Ild , Rr},
	{Ijmp, Rp},
	// others don't have modifiable destination
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
struct Token {
	TokenTypes type;
	string data;
	list<Token> tlist;

	Loc loc;
	bool continued;
	bool firstOnLine;

	Token() {}
	Token(TokenTypes type, string data, Loc loc, bool continued, bool firstOnLine) {
		this->type = type;
		this->data = data;
		this->loc = loc;
		this->continued = continued;
		this->firstOnLine = firstOnLine;
	}
	int64_t asLong() {
		assert(type == Tnumeric);
		return stoi(data);
	}
};
struct Suffix {
	// dest ?<operation>= <reg>/<imm>
	RegNames condReg;
	CondNames cond;
	OpNames modifier;
	RegNames reg;

	Suffix() {
		condReg = Rno;
		cond = Cno;
		modifier = OPno;
		reg = Rno;
	}
};
struct Instr {
	InstrNames instr = InstructionCount;
	Suffix suffixes;
	int immediate;

	string opcodeStr;
	Loc opcodeLoc;
	vector<Token> immFields;
	
	Instr() {}
	bool hasImm() { return immFields.size() != 0; }
	bool hasCond() { return suffixes.cond != Cno; }
	bool hasMod()  { return suffixes.modifier != OPno; }
	bool hasReg()  { return suffixes.reg != Rno; }
	string toStr() {
		string out = opcodeStr;
		for (string s : immAsWords()) {
			out.push_back(' ');
			out.append(s);
		}
		return out;
	}
	vector<string> immAsWords() {
		vector<string> out;
		for (Token t : immFields) {
			if (t.continued) {
				out[out.size()-1].append(t.data);
			} else {
				out.push_back(t.data);
			}
		}
		return out;
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
#define checkReturnValOnFail(cond, message, obj, value) if(!check(cond, message, obj, false)) return value; // TODO make checkReturnOnFail return a default value of the corresponding type making this obsolete

vector<string> errors;
void raiseErrors() { // TODO sort errors based on line and file
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

#define errorQuoted(s) " '" + s + "'"
bool check(bool cond, string message, Token token, bool strict=true) {
	if (!cond) {
		string err = token.loc.toStr() + " ERROR: " + message + errorQuoted(token.data) "\n";
		addError(err, strict);
	}
	return cond;
}
bool check(bool cond, string message, Instr instr, bool strict=true) {
	if (!cond) {
		string err = instr.opcodeLoc.toStr() + " ERROR: " + message + errorQuoted(instr.toStr()) "\n";
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
string getCharRun(string s) {
	int num = isdigit(s.at(0)), alpha = isalpha(s.at(0)), space = isspace(s.at(0));
	if (num == 0 && alpha == 0 && space == 0) return string(1, s.at(0));
	string out;
	int i = 0;
	do {
		out.push_back(s.at(i));
		i++;
	} while (i < s.size() && isdigit(s.at(i)) == num && isalpha(s.at(i)) == alpha && isspace(s.at(i)) == space);
	return out;
}
void addToken(list<Token>& tokens, stack<Token*>& listTokens, Token token) {
	Token* addedPtr;
	if (listTokens.size()) {
		listTokens.top()->tlist.push_back(token);
		addedPtr = &listTokens.top()->tlist.back();
	} else {
		tokens.push_back(token);
		addedPtr = &tokens.back();
	}
	if (token.type == Tlist) {
		listTokens.push(addedPtr);
	}
} 
list<Token> tokenize(ifstream& inFile, string fileName) {
	static_assert(TokenCount == 4, "Exhaustive tokenize definition");
	string line;
	bool continued;
	int firstOnLine;

	list<Token> tokens;
	stack<Token*> listTokens;
	for (int lineNum = 1; getline(inFile, line); ++lineNum) {
		continued = false;
		firstOnLine = 2;
		line = line.substr(0, line.find(';'));
		int col = 1;

		while (line.size()) {
			if (firstOnLine) firstOnLine--;
			char first = line.at(0);
			string run = getCharRun(line);
			col += run.size();
			line = line.substr(run.size());
			if (isspace(first)) {
				continued = false;
				continue;
			}
			Token token;
			Loc loc = Loc(fileName, lineNum, col);
			if (isdigit(first)) {
				token = Token(Tnumeric, run, loc, continued, firstOnLine);
			} else if (isalpha(first)) {
				token = Token(Talpha, run, loc, continued, firstOnLine);
			} else if (run.size() == 1) {
				if (first == '(' || first == '[' || first == '{') {
					token = Token(Tlist, string(1, first), loc, continued, firstOnLine);
					addToken(tokens, listTokens, token);
					continued = false;
					continue;
				} else if (first == ')' || first == ']' || first == '}') {
					checkContinueOnFail(listTokens.size() >= 1, "Unexpected token list termination" errorQuoted(string(1, first)), loc);
					token = *listTokens.top();
					assert(token.type == Tlist && token.data.size() == 1);
					checkContinueOnFail(token.data.at(0) == first - 2 + (first == ')'), "Unmatched token list delimiters" errorQuoted(string(1, first)), loc);
					listTokens.pop();
					continued = true;
					continue;
				} else {
					token = Token(Tspecial, run, loc, continued, firstOnLine);
				}
			} else {
				assert(false);
			}
			continued = true;
			addToken(tokens, listTokens, token);
		}
	}
	check(listTokens.size() == 0, "Unclosed token list", listTokens.size() == 0 ? Token() : *listTokens.top(), false); // TODO better coupling of delim pairs in case of mismatches
	return tokens;
}
// compilation -----------------------------------
string joinTokens(Token top, list<Token>& tokens) {
	string out = top.data;
	while (tokens.size() && tokens.front().continued) {
		out += tokens.front().data;
		tokens.pop_front();
	}
	return out;
}
string joinTokens(list<Token>& tokens) {
	Token top = tokens.front();
	tokens.pop_front();
	return joinTokens(top, tokens);
}
bool extractLabelName(list<Token>& tokens, map<string, Label>& strToLabel, Loc topLoc, int instrNum) {
	string name = joinTokens(tokens);
	checkReturnOnFail(isValidIdentifier(name), "Invalid label name" errorQuoted(name), topLoc);
	checkReturnOnFail(strToLabel.count(name) == 0, "Label redefinition" errorQuoted(name), topLoc);
	strToLabel.insert(pair(name, Label(name, instrNum, topLoc)));
	return true;
}
void _ignoreLine(list<Token>& tokens) {
	while (tokens.size() && !tokens.front().firstOnLine) {
		tokens.pop_front();
	}
}
pair<vector<Instr>, map<string, Label>> compile(list<Token>& tokens, string fileName) {
	vector<Instr> instrs;
	map<string, Label> strToLabel = {{"begin", Label("begin", 0, Loc(fileName, 1, 1))}, {"end", Label("end", 0, Loc(fileName, 1, 1))}};
	if (tokens.size()) {
		strToLabel["begin"].loc = tokens.front().loc;
		strToLabel["end"].loc = tokens.back().loc;
	}
	
	bool ignoreLine = false;
	while (true) {
		if (ignoreLine) _ignoreLine(tokens);
		ignoreLine = true;
		if (tokens.size() == 0) break;

		Token top = tokens.front();
		tokens.pop_front();
		if (top.type == Tspecial) {
			checkContinueOnFail(top.data == ":", "Unsupported special character", top);
			checkContinueOnFail(tokens.size() && !tokens.front().firstOnLine, "Label name expected after", top);
			ignoreLine = false;
			continueOnFalse(extractLabelName(tokens, strToLabel, top.loc, instrs.size()));
		} else if (top.type == Talpha) {
			Instr instr;
			instr.opcodeStr = joinTokens(top, tokens);
			instr.opcodeLoc = top.loc;
			while (tokens.size() && !tokens.front().firstOnLine) {
				instr.immFields.push_back(tokens.front());
				tokens.pop_front();
			}
			instrs.push_back(instr);
		} else {
			checkContinueOnFail(false, "Expected instruction", top);
		}
		ignoreLine = false;
	}
	strToLabel["end"].addr = instrs.size();
	return pair(instrs, strToLabel);
}
// parsing of instructions -----------------------------------------
pair<bool, string> parseCond(Instr& instr, string s) {
	checkReturnValOnFail(s.size() >= 1, "Missing condition", instr, pair(false, ""));
	char reg = s.at(0);
	if (CharToReg.count(reg) == 1) {
		checkReturnValOnFail(reg == 'r' || reg == 'm', "Conditions can test only r/m", instr, pair(false, ""));
		instr.suffixes.condReg = CharToReg[reg];
		s = s.substr(1);
	}
	checkReturnValOnFail(s.size() >= 2, "Missing condition", instr, pair(false, ""));
	string cond = s.substr(0, 2);
	checkReturnValOnFail(StrToCond.count(cond) == 1, "Unknown condition", instr, pair(false, ""));
	instr.suffixes.cond = StrToCond[cond];
	return pair(true, s.substr(2));
}
bool parseSuffixes(Instr& instr, string s, bool condExpected=false) {
	static_assert(RegisterCount == 5 && OperationCount == 10 && ConditionCount == 7, "Exhaustive parseSuffixes definition");
	if (condExpected) {
		instr.suffixes.condReg = Rr; // default
		auto ret = parseCond(instr, s);
		returnOnFalse(ret.first);
		s = ret.second;
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
	static_assert(InstructionCount == 14, "Exhaustive parseInstrOpcode definition");
	for (int checkedLen = min(4, (int)instr.opcodeStr.size()); checkedLen > 0; checkedLen --) { // avoid parsing 'ld' as Il, 'str' as Is, 'swap' as Is and so on
		string substr = instr.opcodeStr.substr(0, checkedLen);
		if (StrToInstr.count(substr) == 1) {
			instr.instr = StrToInstr[substr];
			string suffix = instr.opcodeStr.substr(checkedLen);
			return parseSuffixes(instr, suffix, instr.instr == Ib || instr.instr == Il || instr.instr == Is);
		}
	}
	checkReturnOnFail(false, "Unknown instruction", instr);
	return false;
}
bool parseInstrImmediate(Instr& instr, map<string, Label> &stringToLabel) {
	vector<string> immWords = instr.immAsWords();
	checkReturnOnFail(immWords.size() == 1, "This line has too many fields", instr);
	if (stringToLabel.count(immWords[0]) > 0) {
		instr.immediate = stringToLabel[immWords[0]].addr;
		return true;
	}
	checkReturnOnFail(isdigit(immWords[0].at(0)), "Invalid instruction immediate", instr);
	instr.immediate = stoi(immWords[0]);
	checkReturnOnFail(to_string(instr.immediate) == immWords[0], "Invalid instruction immediate", instr);
	checkReturnOnFail(0 <= instr.immediate && instr.immediate <= WORD_MAX_VAL, "Value of the immediate is out of bounds", instr);
	return true;
}
bool parseInstrFields(Instr& instr, map<string, Label> &stringToLabel) {
	returnOnFalse(parseInstrOpcode(instr));
	if (instr.hasImm()) {
		returnOnFalse(parseInstrImmediate(instr, stringToLabel));
	}
	return true;
}
// checks if the instr has correct combination of suffixes and immediates
bool checkValidity(Instr& instr) {
	static_assert(InstructionCount == 14 && sizeof(Suffix) == 4 * 4, "Exhaustive checkValidity definition");
	if (instr.instr == InstructionCount) unreachable();
	if (instr.instr == Iswap || instr.instr == Iinl) { // check specials
		checkReturnOnFail(!instr.hasImm() && !instr.hasReg(), "This instruction should have no arguments", instr);
	} else if (instr.instr == Iinc || instr.instr == Iinu || instr.instr == Iipc) {
		checkReturnOnFail(!instr.hasImm(), "This instruction cannot have an immediate", instr);
		if (instr.suffixes.reg == Rno) {
			instr.suffixes.reg = Rr; // default
		} else checkReturnOnFail(instr.suffixes.reg == Rr || instr.suffixes.reg == Rm, "Input destination can be only r/m", instr);
	} else {
		checkReturnOnFail(instr.hasImm() ^ instr.hasReg(), "Instrs must have only imm, or 1 reg for now", instr);
	}
	if (InstrToModReg.count(instr.instr) == 0) {
		checkReturnOnFail(!instr.hasMod(), "This instruction cannot have a modifier", instr);
	}
	RegNames condReg = instr.suffixes.condReg; CondNames cond = instr.suffixes.cond;
	if (instr.instr == Ib || instr.instr == Il || instr.instr == Is) {
		checkReturnOnFail((condReg == Rr || condReg == Rm) && cond != Cno, "Invalid condition register or condition", instr);
	} else {
		checkReturnOnFail(condReg == Rno && cond == Cno, "Unexpected condition", instr);
	}
	return true;
}
void parseInstrs(vector<Instr>& instrs, map<string, Label>& strToLabel) {
	for (int i = 0; i < instrs.size(); ++i) {
		Instr instr = instrs[i];
		continueOnFalse(parseInstrFields(instr, strToLabel));
		continueOnFalse(checkValidity(instr));
		instrs[i] = instr;
	}
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
	static_assert(OperationCount == 10, "Exhaustive genOperation definition");
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
	} else if (op == OPshl) {
		outFile << "	shl bx, cl\n"
			"	movzx rcx, bx\n";
	} else if (op == OPshr) {
		outFile << "	shr bx, cl\n"
			"	mov rcx, rbx\n";
	} else if (op == OPbit) {
		outFile << "	shr bx, cl\n"
			"	mov rcx, rbx\n"
			"	and rcx, 1\n";
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
	{Ceq, "sete"},
	{Cne, "setne"},
	{Clt, "sets"},
	{Cle, "setle"},
	{Cgt, "setg"},
	{Cge, "setns"},
};
void genCond(ofstream& outFile, InstrNames instr, RegNames condReg, CondNames cond, int instrNum) {
	static_assert(ConditionCount == 7, "Exhaustive genCond definition");
	genRegisterFetch(outFile, condReg, -1, false);
	if (instr == Ib) {
		outFile << "	cmp bx, 0\n"
		"	" << _jmpInstr[cond] << " instr_" << instrNum + 1 << "\n";
	} else if (instr == Il) {
		outFile <<
			"	xor r15, r15\n"
			"	cmp bx, cx\n"
			"	" << _condLoadInstr[cond] << " r15b\n";
	} else if (instr == Is) {
		outFile <<
			"	xor rax, rax\n"
			"	cmp bx, cx\n"
			"	" << _condLoadInstr[cond] << " al\n"
			"	mov [2*r14+r13], ax\n";
	} else {
		unreachable();
	}
}
void genInstrBody(ofstream& outFile, InstrNames instr, int instrNum, bool inputToR=true) {
	static_assert(InstructionCount == 14, "Exhaustive genInstrBody definition");
	string inputDest = inputToR ? "r15w" : "[2*r14+r13]";

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
	} else if (instr == Il || instr == Is) { // handled in genCond
	} else if (instr == Iswap) {
		outFile << "	mov cx, [2*r14+r13]\n"
			"	mov [2*r14+r13], r15w\n"
			"	mov r15w, cx\n";
	} else if (instr == Ioutu) {
		outFile << "	mov rax, rcx\n"
			"	call print_unsigned\n";
	} else if (instr == Ioutc) {
		outFile <<
			"	mov rdx, stdout_buff\n"
			"	mov [rdx], cl\n"
			"	mov r8, 1\n"
			"	call stdout_write\n";
	} else if (instr == Iinc) {
		outFile << "	call get_next_char\n"
			"	mov " << inputDest << ", dx\n";
	} else if (instr == Iipc) {
		outFile << "	call stdin_peek\n"
			"	mov " << inputDest << ", dx\n";
	} else if (instr == Iinu) {
		string dest = inputToR ? "r15w" : "[2*r14+r13]";
		outFile << "	call input_unsigned\n"
			"	mov " << inputDest << ", ax\n";
	} else if (instr == Iinl) {
		outFile << "	call get_next_char\n"
			"	cmp rdx, 10\n"
			"	jne instr_" << instrNum << "\n";
	} else {
		unreachable();
	}
}
void genAssembly(ofstream& outFile, Instr instr, int instrNum) {	
	// head pos - r14, internal r reg - r15
	// intermediate values - first - rbx, second - rcx
	// addr of cells[0] - r13
	// responsibilities for clamping and register preserving:
	// each operation, register fetch or instr body must touch only appropriate architecture registers, others must be left unchanged
	// also, they need to make sure all changed architecture regs, intermediate value registers and the memory /
	//		is clamped to the right bitsize and contains a valid value
	if (instr.hasImm()) {
		outFile << "	mov rcx, " << instr.immediate << '\n';
	} else if (instr.hasReg()) {
		genRegisterFetch(outFile, instr.suffixes.reg, instrNum);
	}
	if (instr.hasMod()) {
		genRegisterFetch(outFile, InstrToModReg[instr.instr], instrNum, false);
		genOperation(outFile, instr.suffixes.modifier);
	}
	if (instr.hasCond()) {
		genCond(outFile, instr.instr, instr.suffixes.condReg, instr.suffixes.cond, instrNum);
	}
	genInstrBody(outFile, instr.instr, instrNum, instr.suffixes.reg == Rr);
}

void generate(ofstream& outFile, vector<Instr>& instrs) {
	outFile << 
		"extern ExitProcess@4\n"
		"extern GetStdHandle@4\n"
		"extern WriteFile@20\n"
		"extern ReadFile@20\n"
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
		"read_file: ; rcx - fd, rdx - buff, r8 - buff size -> rax - number read\n"
		"	push 0 ; number of bytes read var\n"
		"	mov r9, rsp ; ptr to that var\n"
		"	push 0 ; OVERLAPPED struct null ptr\n"
		"	sub rsp, 32 ; call with shadow space\n"
		"	call ReadFile@20\n"
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
		"stdin_read:\n"
		"	mov rcx, stdin_fd ; get chars into stdin_buff\n"
		"	mov rcx, [rcx]\n"
		"	mov rdx, stdin_buff\n"
		"	mov r8, STDIN_BUFF_SIZE\n"
		"	call read_file\n"
		"\n"
		"	mov rcx, stdin_buff_char_count ; store how many were read\n"
		"	mov [rcx], rax\n"
		"	mov rcx, stdin_buff_chars_read ; 0 chars were processed\n"
		"	mov QWORD [rcx], 0\n"
		"	ret\n"
		"\n"
		"stdin_peek: ; returns next raw stdin char, does not advance read ptr -> rdx char\n"
		"	mov rax, stdin_buff_char_count ; if (char_count == chars_read) fill the stdin_buff\n"
		"	mov rcx, [rax]\n"
		"	mov rax, stdin_buff_chars_read\n"
		"	cmp rcx, [rax]\n"
		"	jne stdin_peek_valid\n"
		"	call stdin_read\n"
		"stdin_peek_valid:\n"
		"	mov rax, stdin_buff_chars_read ; char addr\n"
		"	mov rcx, stdin_buff\n"
		"	add rcx, [rax]\n"
		"\n"
		"	xor rdx, rdx ; peek the char\n"
		"	mov dl, [rcx]\n"
		"	ret\n"
		"get_next_char: ; gets next char from stdin (buffered), advances read ptr -> rdx char\n"
		"	call stdin_peek ; peek the first unread char\n"
		"	mov rax, stdin_buff_chars_read ; eat the char\n"
		"	inc QWORD [rax]\n"
		"	ret\n"
		"\n"
		"utos: ; n - rax -> r8 char count, r9 - char* str\n"
		"	xor r8, r8 ; char count\n"
		"	mov r9, stdout_buff+STDOUT_BUFF_SIZE-1 ; curr buff pos\n"
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
		"input_unsigned: ; consumes all numeric chars, constructs uint out of them -> rax num\n"
		"	xor rax, rax ; out\n"
		"input_unsigned_loop:\n"
		"	push rax\n"
		"	call stdin_peek ; get next char\n"
		"	sub rdx, '0' ; isdigit()\n"
		"	js input_unsigned_end\n"
		"	cmp rdx, 9\n"
		"	jg input_unsigned_end\n"
		"	push rdx\n"
		"\n"
		"	call get_next_char ; eat the valid char\n"
		"	pop rcx ; add to parsed\n"
		"	pop rax\n"
		"	mov r10, 10\n"
		"	mul r10\n"
		"	add rax, rcx\n"
		"	jmp input_unsigned_loop\n"
		"input_unsigned_end:\n"
		"	pop rax\n"
		"	ret\n"
		"\n"
		"global _start\n"
		"_start:\n"
		"	; initialization\n"
		"	call get_std_fds\n"
		"	mov rcx, stdin_buff_char_count\n"
		"	mov QWORD [rcx], 0\n"
		"	mov rcx, stdin_buff_chars_read\n"
		"	mov QWORD [rcx], 0\n"
		"\n"
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
		"	stdout_buff: resb STDOUT_BUFF_SIZE\n"
		"	stdin_buff_chars_read: resq 1\n"
		"	stdin_buff_char_count: resq 1\n"
		"	stdin_buff: resb STDIN_BUFF_SIZE\n"
		"\n"
		"section .data\n"
		"	STDOUT_BUFF_SIZE: EQU " << STDOUT_BUFF_SIZE << "\n"
		"	STDIN_BUFF_SIZE: EQU " << STDIN_BUFF_SIZE << "\n"
		"\n"
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
			"		--keep-asm        - keep the assembly file\n"
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
		} else if (s == "--keep-asm") {
			FLAG_keepAsm = true;
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
	if (!FLAG_keepAsm) {
		removeFile(asmPath);
	} else {
		cout << "[NOTE] asm file: " << asmPath.string() << ":179:1\n";
	}
	removeFile(objectPath);
	if (FLAG_run) {
		runCmdEchoed(exePath.string());
	}
}
int main(int argc, char *argv[]) {
	processLineArgs(argc, argv);
	string fileName = pathFromMasfix(InputFileName).string();

	ifstream inFile = getInputFile();
	list<Token> tokens = tokenize(inFile, fileName);
	inFile.close();
	
	auto compiled = compile(tokens, fileName);
	vector<Instr> instrs = compiled.first;
	map<string, Label> strToLabel = compiled.second;

	parseInstrs(instrs, strToLabel);
	raiseErrors(); // TODO come up with good error continuation strategies for all compilation stages

	ofstream outFile = getOutputFile();
	generate(outFile, instrs);
	outFile.close();

	compileAndRun(OutputFileName);
}
