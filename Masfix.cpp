#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#include <string>
#include <map>
#include <set>
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
#define TOP_NAMESPACE_NAME "__main"

#define MAX_EXPANSION_DEPTH 1024

#define STDOUT_BUFF_SIZE 256
#define STDIN_BUFF_SIZE 256

#define WORD_MAX_VAL 65535
#define CELLS WORD_MAX_VAL+1
// enums --------------------------------
enum TokenTypes {
	Tnumeric,
	Talpha,

	Tspecial,
	Tcolon,
	Tseparator,
	Tlist,

	// intermediate preprocess tokens
	TIexpansion,
	TInamespace,

	TokenCount
};
constexpr int BuiltinDirectivesCount = 3;
const set<string> BuiltinDirectivesSet = {
	"define", 
	"macro",
	"namespace"
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
	char tlistCloseChar() {
		assert(type == Tlist);
		return data.at(0) + 2 - (data.at(0) == '(');
	}
	string toStr() {
		string out = data;
		if (type == Tlist) {
			if (isSeparated()) out.push_back(',');
			out.push_back(tlistCloseChar());
		}
		return out;
	}
	int64_t asLong() { // TODO report value overflows
		assert(type == Tnumeric);
		return stoi(data);
	}
	bool isSeparated() {
		assert(type == Tlist);
		for (Token t : tlist) {
			if (t.type == Tseparator) return true;
		}
		return false;
	}
};
struct Define {
	string name;
	Loc loc;
	string value;

	Define() {}
	Define(string name,	Loc loc, string value) {
		this->name = name;
		this->loc = loc;
		this->value = value;
	}
};
struct MacroArg {
	string name;
	Loc loc;
	stack<list<Token>> value; // TODO test multiple expansions inside each other

	MacroArg() {}
	MacroArg(string name, Loc loc) {
		this->name = name;
		this->loc = loc;
	}
};
struct Macro {
	string name;
	Loc loc;

	map<string, int> nameToArgIdx;
	vector<MacroArg> argList;
	list<Token> body;

	Macro() {}
	Macro(string name, Loc loc) {
		this->name = name;
		this->loc = loc;
	}

	bool hasArg(string name) {
		return nameToArgIdx.count(name);
	}
	void addArg(string name, Loc loc) {
		nameToArgIdx[name] = argList.size();
		argList.push_back(MacroArg(name, loc));
	}
	MacroArg& nameToArg(string name) {
		assert(nameToArgIdx.count(name));
		return argList[nameToArgIdx[name]];
	}
	void addExpansionScope(vector<list<Token>>& fields) {
		for (int i = 0; i < fields.size(); ++i) {
			argList[i].value.push(list(fields[i].begin(), fields[i].end()));
		}
	}
	void closeExpansionScope() {
		for (int i = 0; i < argList.size(); ++i) {
			argList[i].value.pop();
		}
	}
};
struct Namespace {
	string name;
	Loc loc;

	map<string, Define> defines;
	map<string, Macro> macros;

	Namespace() {}
	Namespace(string name, Loc loc) {
		this->name = name;
		this->loc = loc;
	}
};
map<string, Namespace> StrToNamespace;
bool raiseError(string message, Loc loc, bool strict);
struct Scope {
private:
	stack<string> namespaces = stack<string>({ TOP_NAMESPACE_NAME });
	stack<pair<string, string>> macros;
	// TODO better max depth checks - maybe add depth counter
public:
	bool insideArglist=false;
	Scope() {}

	bool insideMacro() {
		return macros.size();
	}
	Namespace& currNamespace() {
		return StrToNamespace[namespaces.top()];
	}
	Namespace& namespaceFromPrefix(string prefix) {
		if (prefix == "") return currNamespace();
		assert(StrToNamespace.count(prefix));
		return StrToNamespace[prefix];
	}
	Macro& currMacro() {
		assert(insideMacro());
		return namespaceFromPrefix(macros.top().first).macros[macros.top().second];
	}

	bool hasDefine(string& namespaceName, string& defineName) {
		return namespaceFromPrefix(namespaceName).defines.count(defineName);
	}
	bool hasMacro(string& namespaceName, string& macroName) {
		return namespaceFromPrefix(namespaceName).macros.count(macroName);
	}
	bool hasMacroArg(string& name) {
		return insideMacro() && currMacro().hasArg(name);
	}

	void addMacroExpansion(string& namespaceName, Token& expansionToken) {
		if (macros.size() > MAX_EXPANSION_DEPTH) {
			raiseError("Maximum expansion depth exceeded", expansionToken.loc, true);
		}
		macros.push(pair(namespaceName, expansionToken.data));
	}
	void endMacroExpansion() {
		currMacro().closeExpansionScope();
		macros.pop();
	}
	void enterNamespace(string currNamespaceName) {
		assert(!insideMacro());
		namespaces.push(currNamespaceName);
	}
	void exitNamespace() {
		assert(namespaces.size() > 1);
		namespaces.pop();
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
	Instr(string opcodeStr,	Loc opcodeLoc) {
		this->opcodeStr = opcodeStr;
		this->opcodeLoc = opcodeLoc;
	}
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
				out[out.size()-1].append(t.toStr());
			} else {
				out.push_back(t.toStr());
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
// globals ---------------------------------
map<string, Label> StrToLabel;

fs::path InputFileName = "";
fs::path OutputFileName = "";

bool FLAG_silent = false;
bool FLAG_run = false;
bool FLAG_keepAsm = false;
bool FLAG_strictErrors = false;
// checks --------------------------------------------------------------------
#define errorQuoted(s) " '" + (s) + "'"
#define unreachable() assert(("Unreachable", false));

#define continueOnFalse(cond) if (!(cond)) continue;
#define returnOnFalse(cond) if (!(cond)) return false;

#define check(cond, message, obj) ((cond) || raiseError(message, obj))
#define checkContinueOnFail(cond, message, obj) if(!check(cond, message, obj)) continue;
#define checkReturnOnFail(cond, message, obj) if(!check(cond, message, obj)) return false;

bool SupressErrors = false;
#define returnOnErrSupress() if (SupressErrors) return false;

vector<string> errors;
void raiseErrors() { // TODO sort errors based on line and file
	for (string s : errors) {
		cerr << s;
	}
	if (errors.size()) exit(1);
}
void addError(string message, bool strict=true) {
	errors.push_back(message);
	if (strict || FLAG_strictErrors) {
		raiseErrors();
	}
}

bool raiseError(string message, Token token, bool strict=false) {
	returnOnErrSupress();
	string err = token.loc.toStr() + " ERROR: " + message + errorQuoted(token.toStr()) "\n";
	addError(err, strict);
	return false;
}
bool raiseError(string message, Instr instr, bool strict=false) {
	returnOnErrSupress();
	string err = instr.opcodeLoc.toStr() + " ERROR: " + message + errorQuoted(instr.toStr()) "\n";
	addError(err, strict);
	return false;
}
bool raiseError(string message, Loc loc, bool strict=false) {
	returnOnErrSupress();
	string err = loc.toStr() + " ERROR: " + message + "\n";
	addError(err, strict);
	return false;
}
// helpers --------------------------------------------------------------
bool _validIdentChar(char c) { return isalnum(c) || c == '_'; }
bool verifyNotInstrOpcode(string name);
bool isValidIdentifier(string name, string errInvalid, Loc& loc) {
	checkReturnOnFail(name.size() > 0 && find_if_not(name.begin(), name.end(), _validIdentChar) == name.end(), errInvalid, loc)
	return check(isalpha(name.at(0)) || name.at(0) == '_', errInvalid, loc);
}
bool checkIdentRedefinitons(string name, Loc& loc, bool label, string currNamespaceName="") {
	checkReturnOnFail(verifyNotInstrOpcode(name), "Name shadows an instruction" errorQuoted(name), loc);
	checkReturnOnFail(BuiltinDirectivesSet.count(name) == 0, "Name shadows a builtin directive" errorQuoted(name), loc)
	if (label) {
		checkReturnOnFail(StrToLabel.count(name) == 0, "Label redefinition" errorQuoted(name), loc);
	} else {
		assert(currNamespaceName != "");
		Namespace& currNamespace = StrToNamespace[currNamespaceName];
		checkReturnOnFail(currNamespace.defines.count(name) == 0, "Define redefinition" errorQuoted(name), loc);
		checkReturnOnFail(currNamespace.macros.count(name) == 0, "Macro redefinition" errorQuoted(name), loc);
		checkReturnOnFail(StrToNamespace.count(name) == 0, "Namespace redefinition" errorQuoted(name), loc);
	}
	return true;
}
void eatLine(list<Token>& tokens, list<Token>::iterator& itr) {
	while (itr != tokens.end() && !itr->firstOnLine) {
		itr = tokens.erase(itr);
	}
}
Token eraseToken(list<Token>& l, list<Token>::iterator& itr) {
	Token out = *itr;
	itr = l.erase(itr);
	return out;
}
bool eatToken(list<Token>& currList, list<Token>::iterator& itr, Loc errLoc, Token& outToken, TokenTypes type, string errMissing, bool sameLine) {
	static_assert(TokenCount == 8, "Exhaustive eatToken definition");
	checkReturnOnFail(itr != currList.end(), errMissing, errLoc);
	if (sameLine) checkReturnOnFail(!itr->firstOnLine, errMissing, errLoc);
	outToken = *itr;
	eraseToken(currList, itr);
	return check(outToken.type == type, "Unexpected token type", outToken); // TODO more specific err message here
}
void eatTokenRun(list<Token>& currList, list<Token>::iterator& itr, string& name, Loc& loc, bool canStartLine=true, int eatAnything=0) {
	static_assert(TokenCount == 8, "Exhaustive eatTokenRun definition");
	if (itr != currList.end()) loc = itr->loc;
	bool first = true; name = "";

	set<TokenTypes> allowedTypes = {Tnumeric, Talpha, Tspecial};
	if (eatAnything >= 1) allowedTypes.merge(set({Tcolon, Tseparator}));
	if (eatAnything == 2) allowedTypes.insert(Tlist);
	while (itr != currList.end() && (!itr->firstOnLine || (canStartLine && first)) && (first || itr->continued) && allowedTypes.count(itr->type)) {
		name += itr->toStr();
		eraseToken(currList, itr);
		first = false;
	}
}
bool eatIdentifier(list<Token>& currList, list<Token>::iterator& itr, string& name, Loc& loc, string identPurpose, bool canStartLine=false, int eatAnything=0) {
	Loc prevLoc = loc;
	eatTokenRun(currList, itr, name, loc, canStartLine, eatAnything);
	return check(name != "", "Missing " + identPurpose + " name", prevLoc);
}
bool eatComplexIdentifier(list<Token>& currList, list<Token>::iterator& itr, Loc loc, string& ident, string purpose) {
	string fragment; Loc fragLoc = loc; bool canStartLine = purpose == "instr";
	checkReturnOnFail(itr != currList.end() && (!itr->firstOnLine || canStartLine), "Missing " + purpose + " name", loc);
	do {
		if (itr->type == Tlist) {
			checkReturnOnFail(itr->tlist.size(), "Expected " + purpose + " field", itr->loc);
			list<Token>::iterator tlistItr = itr->tlist.begin();
			returnOnFalse(eatIdentifier(itr->tlist, tlistItr, fragment, fragLoc, purpose + " field", canStartLine, 2));
			checkReturnOnFail(tlistItr == itr->tlist.end(), "Simple " + purpose + " field expected", itr->loc);
			ident.append(fragment);
			++itr;
		} else {
			returnOnFalse(eatIdentifier(currList, itr, fragment, itr->loc, purpose + " field", canStartLine, 1));
			ident.append(fragment);
		}
	} while (itr != currList.end() && itr->continued);
	if (purpose == "instr") return true;
	returnOnFalse(isValidIdentifier(ident, "Invalid " + purpose + " name" errorQuoted(ident), loc));
	return checkIdentRedefinitons(ident, loc, true);
}
// tokenization -----------------------------------------------------------------
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
	static_assert(TokenCount == 8, "Exhaustive tokenize definition");
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
			Token token;
			Loc loc = Loc(fileName, lineNum, col);
			col += run.size();
			line = line.substr(run.size());
			if (isspace(first)) {
				continued = false;
				firstOnLine ++;
				continue;
			}
			if (isdigit(first)) {
				token = Token(Tnumeric, run, loc, continued, firstOnLine);
			} else if (isalpha(first)) {
				token = Token(Talpha, run, loc, continued, firstOnLine);
			} else if (run.size() == 1) {
				if (first == '(' || first == '[' || first == '{') {
					token = Token(Tlist, string(1, first), loc, continued, firstOnLine);
					addToken(tokens, listTokens, token);
					continued = false; // TODO should (first token in list).firstOnLine - always / never / depends?
					continue; // TODO allow first token in list to continue??
				} else if (first == ')' || first == ']' || first == '}') {
					checkContinueOnFail(listTokens.size() >= 1, "Unexpected token list termination" errorQuoted(string(1, first)), loc);
					token = *listTokens.top();
					assert(token.type == Tlist);
					checkContinueOnFail(first == token.tlistCloseChar(), "Unmatched token list delimiters" errorQuoted(string(1, first)), loc);
					listTokens.pop();
					continued = true;
					continue;
				} else if (first == ':') {
					token = Token(Tcolon, run, loc, continued, firstOnLine);
				} else if (first == ',') {
					token = Token(Tseparator, run, loc, continued, firstOnLine);
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
	while (listTokens.size()) {
		check(false, "Unclosed token list", *listTokens.top());
		listTokens.pop();
	}
	return tokens;
}
// preprocess -------------------------------------------------------------------------
#define directiveEatIdentifier(identPurpose, definiton) \
	returnOnFalse(eatIdentifier(currList, itr, name, loc, identPurpose, false)); \
	returnOnFalse(isValidIdentifier(name, "Invalid " + string(identPurpose) + " name" errorQuoted(name), loc)); \
	if (definiton) returnOnFalse(checkIdentRedefinitons(name, loc, false, scope.currNamespace().name));
#define directiveEatToken(type, missingErr, sameLine) returnOnFalse(eatToken(currList, itr, loc, token, type, missingErr, sameLine));
void insertToken(list<Token>& currList, list<Token>::iterator& itr, Token& token) {
	itr = currList.insert(itr, token);
}
void insertToken(list<Token>& currList, list<Token>::iterator& itr, Token&& token) {
	itr = currList.insert(itr, token);
}
void insertList(list<Token>& currList, list<Token>::iterator& itr, list<Token>& tlist, Token& percentToken) {
	assert(tlist.size());
	list<Token>::iterator tempItr = itr;
	itr = currList.insert(tempItr, tlist.front());
	itr->continued = percentToken.continued;
	itr->firstOnLine = percentToken.firstOnLine;
	for (auto itr = ++tlist.begin(); itr != tlist.end(); ++itr) {
		currList.insert(tempItr, *itr);
	}
}
vector<pair<list<Token>, Loc>> splitTlist(Token& tlist) {
	assert(tlist.type == Tlist);
	if (tlist.tlist.size() == 0) return vector<pair<list<Token>, Loc>>();
	Loc lastSepLoc = tlist.loc;
	vector<pair<list<Token>, Loc>> splits(1, pair<list<Token>, Loc>(list<Token>(), lastSepLoc));
	for (Token& t : tlist.tlist) {
		if (t.type == Tseparator) {
			lastSepLoc = t.loc;
			splits.push_back(pair<list<Token>, Loc>(list<Token>(), lastSepLoc));
		} else {
			splits.back().first.push_back(t);
		}
	}
	return splits;
}
bool arglistFromTlist(Token& tlist, Scope& scope, Macro& mac) {
	assert(tlist.type == Tlist);
	string name; list<Token> currList; Loc loc;
	for (auto [currList, loc] : splitTlist(tlist)) {
		list<Token>::iterator itr = currList.begin();

		directiveEatIdentifier("macro arg", true);
		checkReturnOnFail(itr == currList.end(), "Separator expected", itr->loc);
		checkReturnOnFail(mac.nameToArgIdx.count(name) == 0, "Macro argument redefinition" errorQuoted(name), loc);
		mac.addArg(name, loc);
	}
	return true;
}
bool processMacroDef(list<Token>& currList, list<Token>::iterator& itr, Scope& scope, string name, Loc loc, Loc percentLoc) {
	Token token;
	scope.currNamespace().macros[name] = Macro(name, percentLoc);
	Macro& mac = scope.currNamespace().macros[name];
	directiveEatToken(Tlist, "Macro arglist expected", true);
	returnOnFalse(arglistFromTlist(token, scope, mac));
	directiveEatToken(Tlist, "Macro body expected", false);
	checkReturnOnFail(!token.isSeparated(), "No separators expected in macro body", loc);
	mac.body = list(token.tlist.begin(), token.tlist.end());
	return true;
}
bool processNamespaceDef(list<Token>& currList, list<Token>::iterator& itr, string name, Loc loc, Loc percentLoc) {
	StrToNamespace[name] = Namespace(name, percentLoc);
	Token token;
	directiveEatToken(Tlist, "Namespace body expected", true);
	insertToken(currList, itr, Token(TInamespace, name, percentLoc, false, true));
	itr->tlist = token.tlist;
	return true;
}
bool processDirectiveDef(list<Token>& currList, list<Token>::iterator& itr, string directive, Scope& scope, Token percentToken, Loc directiveLoc) {
	static_assert((BuiltinDirectivesCount == 3, "Exhaustive processDirectiveDef definiton"));
	string name; Loc loc = directiveLoc; Token token;
	checkReturnOnFail(percentToken.firstOnLine, "Unexpected directive here" errorQuoted(directive), percentToken.loc);
	directiveEatIdentifier(directive, true);
	if (directive == "define") {
		directiveEatToken(Tnumeric, "Define value expected", true);
		scope.currNamespace().defines[name] = Define(name, percentToken.loc, token.data);
	} else if (directive == "macro") {
		returnOnFalse(processMacroDef(currList, itr, scope, name, loc, percentToken.loc));
	} else if (directive == "namespace") {
		returnOnFalse(processNamespaceDef(currList, itr, name, loc, percentToken.loc));
	} else {
		unreachable();
	}
	return check(itr == currList.end() || itr->firstOnLine, "Unexpected token after directive", *itr);
}
void expandDefineUse(list<Token>& currList, list<Token>::iterator& itr, Token percentToken, Scope& scope, string namespaceName, string defineName) {
	Define& define = scope.namespaceFromPrefix(namespaceName).defines[defineName];
	Token expanded = Token(Tnumeric, define.value, percentToken.loc, false, percentToken.firstOnLine);
	insertToken(currList, itr, expanded);
}
bool recursivelyProcessArglist(list<Token> &tokens, Scope &scope);
bool processExpansionArglist(Token& tlist, Scope& scope, Macro& mac) {
	assert(tlist.type == Tlist);
	auto splits = splitTlist(tlist);
	checkReturnOnFail(splits.size() == mac.argList.size(), "Unexpected number of expansion arguments", tlist.loc);
	list<Token> currList; Loc loc; int idx = 0; vector<list<Token>> expanded;
	for (auto [currList, loc] : splits) {
		checkReturnOnFail(currList.size() > 0, "Expansion argument expected", loc);
		currList.front().continued = false;
		returnOnFalse(recursivelyProcessArglist(currList, scope)); // the hyper loop closes
		expanded.push_back(currList);
		++idx;
	}
	mac.addExpansionScope(expanded);
	return true;
}
bool expandMacroUse(list<Token>& currList, list<Token>::iterator& itr, Loc loc, Scope& scope, string namespaceName, string macroName) {
	Macro& mac = scope.namespaceFromPrefix(namespaceName).macros[macroName]; Token token;
	directiveEatToken(Tlist, "Expansion arglist expected", true);
	returnOnFalse(processExpansionArglist(token, scope, mac));
	checkReturnOnFail(itr == currList.end() || itr->firstOnLine, "Unexpected token after macro use", *itr);
	
	Token expanded = Token(TIexpansion, macroName, mac.loc, false, true);
	expanded.tlist = list(mac.body.begin(), mac.body.end());
	scope.addMacroExpansion(namespaceName, expanded);
	insertToken(currList, itr, expanded);
	return true;
}
bool processDirectivePrefix(list<Token>& currList, list<Token>::iterator& itr, string& namespaceName, string& directiveName, Loc& loc, Scope& scope, bool& notContinued) {
	string name; namespaceName = "";
	directiveEatIdentifier("directive", false);
	if (itr != currList.end() && itr->type == Tcolon && !itr->firstOnLine) {
		checkReturnOnFail(StrToNamespace.count(name), "Invalid namespace" errorQuoted(name), loc);
		namespaceName = name;
		eraseToken(currList, itr);
		directiveEatIdentifier("directive", false);
	}
	directiveName = name;
	notContinued = itr == currList.end() || !itr->continued;
	return true;
}
bool processDirective(list<Token>& currList, list<Token>::iterator& itr, Token percentToken, Scope& scope) {
	static_assert(TokenCount == 8, "Exhaustive processDirective definition");
	string namespaceName, directiveName; Loc loc = percentToken.loc; bool notContinued;
	returnOnFalse(processDirectivePrefix(currList, itr, namespaceName, directiveName, loc, scope, notContinued));	
	if (BuiltinDirectivesSet.count(directiveName)) {
		checkReturnOnFail(namespaceName == "", "Unexpected namespace prefix", percentToken.loc);
		checkReturnOnFail(notContinued, "Unexpected continued token", *itr);
		checkReturnOnFail(!scope.insideArglist, "Definition not allowed inside arglist", percentToken.loc);
		checkReturnOnFail(!scope.insideMacro(), "Definition not allowed inside macro", percentToken.loc);
		returnOnFalse(processDirectiveDef(currList, itr, directiveName, scope, percentToken, loc));
	} else if (scope.hasDefine(namespaceName, directiveName)) {
		checkReturnOnFail(notContinued, "Unexpected continued token", *itr);
		expandDefineUse(currList, itr, percentToken, scope, namespaceName, directiveName);
	} else if (scope.hasMacro(namespaceName, directiveName)) {
		checkReturnOnFail(percentToken.firstOnLine && !scope.insideArglist, "Unexpected macro use", percentToken.loc);
		returnOnFalse(expandMacroUse(currList, itr, percentToken.loc, scope, namespaceName, directiveName));
	} else if (scope.hasMacroArg(directiveName)) {
		checkReturnOnFail(namespaceName == "", "Unexpected namespace prefix", percentToken.loc);
		checkReturnOnFail(notContinued, "Unexpected continued token", *itr);
		list<Token>& argField = scope.currMacro().nameToArg(directiveName).value.top();
		insertList(currList, itr, argField, percentToken);
	} else {
		checkReturnOnFail(false, "Undeclared identifier" errorQuoted(directiveName), loc);
	}
	return true;
}
#define eatLineOnFalse(cond) if (!(cond)) { eatLine(*currList, *currItr); errorLess = false; continue; }
#define addNewList() openLists.push(currToken); \
	openItrs.push(currToken.tlist.begin()); \
	currList = &openLists.top().get().tlist; \
	currItr = &openItrs.top();
bool preprocessImpl(list<Token>& tokens, Scope& scope) {
	stack<reference_wrapper<Token>> openLists;
	stack<list<Token>::iterator> openItrs({ tokens.begin() });
	bool errorLess = true;

	while (openItrs.size()) {
		list<Token>* currList = openLists.size() ? &openLists.top().get().tlist : &tokens;
		list<Token>::iterator* currItr = &openItrs.top();
		while (*currItr != currList->end()) {
			Token& currToken = **currItr;
			if (currToken.type == Tspecial && currToken.data == "%") {
				eatLineOnFalse(check(!currToken.continued, "Directive must not continue", currToken.loc));
				Token percentToken = eraseToken(*currList, *currItr);
				eatLineOnFalse(processDirective(*currList, *currItr, percentToken, scope));
				continue;
			}
			++ (*currItr);
			if (currToken.type == Tlist) {
				addNewList();
			} else if (currToken.type == TIexpansion) {
				addNewList();
			} else if (currToken.type == TInamespace) {
				scope.enterNamespace(currToken.data);
				addNewList();
			}
		}
		if (openLists.size()) {
			if (openLists.top().get().type == TIexpansion) {
				scope.endMacroExpansion();
			} else if (openLists.top().get().type == TInamespace) {
				scope.exitNamespace();
			}
			openLists.pop();
		}
		openItrs.pop();
	}
	return errorLess;
}
bool recursivelyProcessArglist(list<Token>& tokens, Scope& scope) {
	scope.insideArglist = true;
	bool preprocessRetval = preprocessImpl(tokens, scope);
	scope.insideArglist = false;
	return preprocessRetval;
}
void preprocess(list<Token>& tokens, string fileName) {
	StrToNamespace[TOP_NAMESPACE_NAME] = Namespace(TOP_NAMESPACE_NAME, Loc(fileName, 0, 0));
	Scope scope;
	preprocessImpl(tokens, scope);
}
// compilation -----------------------------------
void initStrToLabel(list<Token>& tokens, string fileName) {
	StrToLabel = {{"begin", Label("begin", 0, Loc(fileName, 1, 1))}, {"end", Label("end", 0, Loc(fileName, 1, 1))}};
	if (tokens.size()) {
		StrToLabel["begin"].loc = tokens.front().loc;
		StrToLabel["end"].loc = tokens.back().loc;
	}
}
vector<Instr> compile(list<Token>& tokens, string fileName) {
	list<Token>::iterator itr = tokens.begin();
	vector<Instr> instrs;
	initStrToLabel(tokens, fileName);
	
	bool ignoreLine = false;
	bool labelOnLine;
	while (true) {
		if (ignoreLine) eatLine(tokens, itr);
		ignoreLine = true;
		if (itr == tokens.end()) break;

		Token top = *(itr++);
		labelOnLine = labelOnLine && !top.firstOnLine;
		if (top.type == Tcolon) {
			string name;
			continueOnFalse(eatComplexIdentifier(tokens, itr, top.loc, name, "label"));
			ignoreLine = false;
			checkContinueOnFail(!labelOnLine, "Max one label per line", top.loc);
			labelOnLine = true;
			StrToLabel.insert(pair(name, Label(name, instrs.size(), top.loc)));
		} else if (top.type == Talpha) {
			string instrStr = "";
			continueOnFalse(eatComplexIdentifier(tokens, --itr, top.loc, instrStr, "instr"));
			instrs.push_back(Instr(instrStr, top.loc));
			while (itr != tokens.end() && !itr->firstOnLine) {
				instrs.back().immFields.push_back(*(itr++));
			}
		} else if (top.type == TIexpansion || top.type == TInamespace) {
			list<Token>::iterator expansion = --itr;
			tokens.splice(++itr, top.tlist);
			itr = ++expansion;
		} else {
			checkContinueOnFail(false, "Unexpected token", top);
		}
		ignoreLine = false;
	}
	StrToLabel["end"].addr = instrs.size();
	return instrs;
}
// parsing -------------------------------------------------------------------
bool parseCond(Instr& instr, string& s) {
	checkReturnOnFail(s.size() >= 1, "Missing condition", instr);
	char reg = s.at(0);
	if (CharToReg.count(reg) == 1) {
		checkReturnOnFail(reg == 'r' || reg == 'm', "Conditions can test only r/m", instr);
		instr.suffixes.condReg = CharToReg[reg];
		s = s.substr(1);
	}
	checkReturnOnFail(s.size() >= 2, "Missing condition", instr);
	string cond = s.substr(0, 2);
	s = s.substr(2);
	checkReturnOnFail(StrToCond.count(cond) == 1, "Unknown condition", instr);
	instr.suffixes.cond = StrToCond[cond];
	return true;
}
bool parseSuffixes(Instr& instr, string s, bool condExpected=false) {
	static_assert(RegisterCount == 5 && OperationCount == 10 && ConditionCount == 7, "Exhaustive parseSuffixes definition");
	if (condExpected) {
		instr.suffixes.condReg = Rr; // default
		returnOnFalse(parseCond(instr, s));
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
	return check(false, "Unknown instruction", instr);
}
bool verifyNotInstrOpcode(string name) {
	Instr instr;
	instr.opcodeStr = name;
	SupressErrors = true;
	bool ans = !parseInstrOpcode(instr);
	SupressErrors = false;
	return ans;
}
bool parseInstrImmediate(Instr& instr) {
	vector<string> immWords = instr.immAsWords();
	checkReturnOnFail(immWords.size() == 1, "Only simple immediates for now", instr);
	if (StrToLabel.count(immWords[0]) > 0) {
		instr.immediate = StrToLabel[immWords[0]].addr;
		return true;
	}
	checkReturnOnFail(isdigit(immWords[0].at(0)), "Invalid instruction immediate", instr);
	instr.immediate = stoi(immWords[0]);
	checkReturnOnFail(to_string(instr.immediate) == immWords[0], "Invalid instruction immediate", instr);
	return check(0 <= instr.immediate && instr.immediate <= WORD_MAX_VAL, "Value of the immediate is out of bounds", instr);
}
bool parseInstrFields(Instr& instr) {
	returnOnFalse(parseInstrOpcode(instr));
	if (instr.hasImm()) {
		returnOnFalse(parseInstrImmediate(instr));
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
void parseInstrs(vector<Instr>& instrs) {
	for (int i = 0; i < instrs.size(); ++i) {
		Instr instr = instrs[i];
		continueOnFalse(parseInstrFields(instr));
		continueOnFalse(checkValidity(instr));
		instrs[i] = instr;
	}
	check(instrs.size() <= WORD_MAX_VAL+1, "The instruction count " + to_string(instrs.size()) + " exceeds WORD_MAX_VAL=" + to_string(WORD_MAX_VAL), instrs[instrs.size()-1].opcodeLoc);
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
void checkCond(bool cond, string message) {
	if (!cond) {
		string err = "ERROR: " + message + "\n";
		addError(err, true);
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
			checkUsage(false, "Unknown command line arg" errorQuoted(s));
		}
	}
	genFileNames(args[args.size()-1]);
}
ifstream getInputFile() {
	ifstream inFile;
	checkCond(fs::exists(InputFileName), "The input file" errorQuoted(InputFileName.string()) " couldn't be found");
	inFile.open(InputFileName);
	checkCond(inFile.good(), "The input file" errorQuoted(InputFileName.string()) " couldn't be opened");
	return inFile;
}
ofstream getOutputFile() {
	ofstream outFile;
	outFile.open(OutputFileName);
	checkCond(outFile.good(), "The output file" errorQuoted(OutputFileName.string()) " couldn't be opened");
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
int main(int argc, char *argv[]) {
	processLineArgs(argc, argv);
	string fileName = pathFromMasfix(InputFileName).string();

	ifstream inFile = getInputFile();
	list<Token> tokens = tokenize(inFile, fileName);
	inFile.close();
	
	preprocess(tokens, fileName);

	vector<Instr> instrs = compile(tokens, fileName);

	parseInstrs(instrs);
	raiseErrors();

	ofstream outFile = getOutputFile();
	generate(outFile, instrs);
	outFile.close();

	compileAndRun(OutputFileName);
}
