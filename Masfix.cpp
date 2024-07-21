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
	TImodule,
	TIexpansion,
	TInamespace,
	TIarglist,

	TokenCount
};
constexpr int DefiningDirectivesCount = 3;
const set<string> DefiningDirectivesSet = {
	"define",
	"macro",
	"namespace"
};
constexpr int BuiltinDirectivesCount = 1;
const set<string> BuiltinDirectivesSet = {
	"using"
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
	void addExpansionArg(int argIdx, list<Token>::iterator from, list<Token>::iterator to) {
		if (argIdx >= argList.size()) return;
		argList[argIdx].value.push(list<Token>(from, to));
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

	map<string, int> innerNamespaces;
	set<int> usedNamespaces;
	int upperNamespaceId;
	bool isUpperAccesible;

	Namespace() {}
	Namespace(string name, Loc loc, int upperNamespaceId, bool isUpperAccesible=true) {
		this->name = name;
		this->loc = loc;
		this->upperNamespaceId = upperNamespaceId;
		this->isUpperAccesible = isUpperAccesible;
	}
};

int NextNamespaceId = 0;
map<int, Namespace> IdToNamespace;
bool raiseError(string message, Loc loc, bool strict);
/// responsible for iterating tokens and nested tlists,
/// keeping track of current module, namespace, expansion scope, arglist situation
struct Scope {
private:
	stack<int> namespaces = stack<int>({ 0 });
	stack<pair<int, string>> macros;
	// TODO better max depth checks - maybe add depth counter
	stack<reference_wrapper<Token>> tlists;
	stack<list<Token>::iterator> itrs;

	/// opens new tlist for iteration
	void openList(Token& tlist) {
		tlists.push(tlist);
		itrs.push(tlist.tlist.begin());
	}
	/// closes single list
	void closeList() {
		assert(!hasNext());
		if (tlists.top().get().type == TIexpansion) {
			endMacroExpansion();
		} else if (tlists.top().get().type == TInamespace) {
			exitNamespace();
		}
		tlists.pop(); itrs.pop();
	}

public:
	Scope() {}
	Scope(Token& tlistToken) {
		init(tlistToken);
	}
	void init(Token& tlistToken) {
		static_assert(sizeof(Scope) == 320, "Exhaustive Scope.init definition");
		assert(!tlists.size());
		assert(!itrs.size());
		assert(!macros.size());
		assert(namespaces.size() == 1);
		tlists.push(tlistToken);
		itrs.push(currList().begin());
	}
	list<Token>& currList() {
		return tlists.top().get().tlist;
	}
	Token& currToken() {
		return *itrs.top();
	}

	/// whether currList has any next token
	bool hasNext() {
		return itrs.top() != currList().end();
	}
	/// closes ended lists if necessary, returns if iteration can continue in this situation
	bool advanceIteration() {
		while (tlists.size() && !hasNext() && !insideArglist()) closeList();
		return tlists.size() && hasNext();
	}
	/// advances iteration inside current list
	list<Token>::iterator& next() {
		return ++itrs.top();
	}
	/// advances iteration, opens new nested list if provided
	list<Token>::iterator& next(Token& tlist) {
		++itrs.top();
		if (tlist.type == Tlist || tlist.type == TIexpansion || tlist.type == TInamespace) {
			openList(tlist);
		}
		return itrs.top();
	}
// situation checks -----------------------------------------
	bool insideMacro() {
		return macros.size();
	}
	/// is closest tlist an arglist
	bool insideArglist() {
		return tlists.top().get().type == TIarglist;
	}
	int currNamespaceId() {
		return insideMacro() ? macros.top().first : namespaces.top();
	}
	Namespace& currNamespace() {
		return IdToNamespace[currNamespaceId()];
	}
	Macro& currMacro() {
		assert(insideMacro());
		return currNamespace().macros[macros.top().second];
	}
	bool hasMacroArg(string& name) {
		return insideMacro() && currMacro().hasArg(name);
	}
	void addMacroExpansion(int namespaceId, Token& expansionToken) {
		if (macros.size() > MAX_EXPANSION_DEPTH) {
			raiseError("Maximum expansion depth exceeded", expansionToken.loc, true);
		}
		macros.push(pair(namespaceId, expansionToken.data));
	}
	void endMacroExpansion() {
		currMacro().closeExpansionScope();
		macros.pop();
	}
	void addInnerNamespace(string& name, Loc percentLoc) {
		assert(!insideMacro());
		IdToNamespace[NextNamespaceId] = Namespace(name, percentLoc, currNamespaceId());
		currNamespace().innerNamespaces[name] = NextNamespaceId;
		namespaces.push(NextNamespaceId++);
	}
	void exitNamespace() {
		assert(namespaces.size() > 1);
		namespaces.pop();
	}
	void enterArglist(Token& tlist) {
		assert(!insideArglist());
		tlist.type = TIarglist;
		openList(tlist);
	}
	void exitArglist() {
		assert(insideArglist());
		closeList();
	}

// helpers -------------------------------------------------
	void insertToken(Token& token) {
		itrs.top() = currList().insert(itrs.top(), token);
	}
	void insertToken(Token&& token) {
		itrs.top() = currList().insert(itrs.top(), token);
	}
	void insertList(list<Token>& tlist, Token& percentToken) {
		assert(tlist.size());
		itrs.top() = currList().insert(itrs.top(), tlist.begin(), tlist.end());
		itrs.top()->continued = percentToken.continued;
		itrs.top()->firstOnLine = percentToken.firstOnLine;
	}
	Token eatenToken() {
		Token t = currToken();
		itrs.top() = currList().erase(itrs.top());
		return t;
	}
	/// eats tokens upto EOL or separator 
	void eatLine() {
		while (hasNext() && !currToken().firstOnLine && currToken().type != Tseparator) {
			eatenToken();
		}
	}

	#define _addMacroArg() \
		if (argStart != itrs.top()) mac.addExpansionArg(count++, argStart, itrs.top()); \
		else errorLess = raiseError("Argument expected", loc, false);
	bool sliceArglist(Macro& mac, Loc& loc, int& count) {
		// TODO inconsistent macro arg values on error
		itrs.top() = currList().begin();
		list<Token>::iterator argStart = itrs.top();
		bool errorLess = true;
		// TODO inject error checking for arg count
		while (hasNext()) {
			if (currToken().type == Tseparator) {
				_addMacroArg();
				loc = currToken().loc;
				argStart = next();
			} else {	
				loc = currToken().loc;
				next();
			}
		}
		_addMacroArg();
		return errorLess;
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
bool _validIdentChar(char c) { return isalnum(c) || c == '_'; }
bool verifyNotInstrOpcode(string name);
bool isValidIdentifier(string name, string errInvalid, Loc& loc) {
	checkReturnOnFail(name.size() > 0 && find_if_not(name.begin(), name.end(), _validIdentChar) == name.end(), errInvalid, loc)
	return check(isalpha(name.at(0)) || name.at(0) == '_', errInvalid, loc);
}
bool checkIdentRedefinitions(string name, Loc& loc, bool label, Namespace* currNamespace=nullptr) {
	checkReturnOnFail(verifyNotInstrOpcode(name), "Name shadows an instruction" errorQuoted(name), loc);
	checkReturnOnFail(!DefiningDirectivesSet.count(name) && !BuiltinDirectivesSet.count(name), "Name shadows a builtin directive" errorQuoted(name), loc)
	if (label) {
		checkReturnOnFail(StrToLabel.count(name) == 0, "Label redefinition" errorQuoted(name), loc);
	} else {
		assert(currNamespace);
		checkReturnOnFail(currNamespace->defines.count(name) == 0, "Define redefinition" errorQuoted(name), loc);
		checkReturnOnFail(currNamespace->macros.count(name) == 0, "Macro redefinition" errorQuoted(name), loc);
		checkReturnOnFail(currNamespace->innerNamespaces.count(name) == 0, "Namespace redefinition" errorQuoted(name), loc);
	}
	return true;
}
#define processArglistWrapper(body) \
	scope.enterArglist(token); \
	body \
	scope.exitArglist(); \
	returnOnFalse(retval);
bool eatToken(Scope& scope, Loc& loc, Token& outToken, TokenTypes type, string errMissing, bool sameLine) {
	static_assert(TokenCount == 10, "Exhaustive eatToken definition");
	checkReturnOnFail(scope.hasNext(), errMissing, loc);
	if (sameLine) checkReturnOnFail(!scope.currToken().firstOnLine, errMissing, loc);
	outToken = scope.eatenToken(); loc = outToken.loc;
	return check(outToken.type == type, "Unexpected token type", outToken); // TODO more specific err message here
}
void eatTokenRun(Scope& scope, string& name, Loc& loc, bool canStartLine=true, int eatAnything=0) {
	static_assert(TokenCount == 10, "Exhaustive eatTokenRun definition");
	if (scope.hasNext()) loc = scope.currToken().loc;
	bool first = true; name = "";

	set<TokenTypes> allowedTypes = {Tnumeric, Talpha, Tspecial};
	if (eatAnything >= 1) allowedTypes.insert(Tcolon);
	if (eatAnything == 2) allowedTypes.insert(Tlist);
	while (scope.hasNext() && (!scope.currToken().firstOnLine || (canStartLine && first)) && (first || scope.currToken().continued) && allowedTypes.count(scope.currToken().type)) {
		name += scope.eatenToken().toStr();
		first = false;
	}
}
bool eatIdentifier(Scope& scope, string& name, Loc& loc, string identPurpose, bool canStartLine=false, int eatAnything=0) {
	Loc prevLoc = loc;
	eatTokenRun(scope, name, loc, canStartLine, eatAnything);
	return check(name != "", "Missing " + identPurpose + " name", prevLoc);
}
#define directiveEatIdentifier(identPurpose, definition, eatAnything) \
	returnOnFalse(eatIdentifier(scope, name, loc, identPurpose, false, eatAnything)); \
	returnOnFalse(isValidIdentifier(name, "Invalid " + string(identPurpose) + " name" errorQuoted(name), loc)); \
	if (definition) returnOnFalse(checkIdentRedefinitions(name, loc, false, &scope.currNamespace()));
#define directiveEatToken(type, missingErr, sameLine) returnOnFalse(eatToken(scope, loc, token, type, missingErr, sameLine));
bool arglistFromTlist(Scope& scope, Loc& loc, Macro& mac) {
	string name; bool first = true;
	while (scope.hasNext()) {
		if (!first) {
			Token sep = scope.eatenToken(); loc = sep.loc;
			checkReturnOnFail(sep.type == Tseparator, "Separator expected", loc);
		}
		first = false;
		directiveEatIdentifier("macro arg", true, 1);
		checkReturnOnFail(mac.nameToArgIdx.count(name) == 0, "Macro argument redefinition" errorQuoted(name), loc);
		mac.addArg(name, loc);
	}
	return true;
}
bool processMacroDef(Scope& scope, string name, Loc loc, Loc percentLoc) {
	Token token;
	scope.currNamespace().macros[name] = Macro(name, percentLoc);
	Macro& mac = scope.currNamespace().macros[name];
	directiveEatToken(Tlist, "Macro arglist expected", true);
	if (token.tlist.size()) {
		processArglistWrapper( bool retval = arglistFromTlist(scope, loc, mac); )
	}
	directiveEatToken(Tlist, "Macro body expected", false);
	checkReturnOnFail(!token.isSeparated(), "No separators expected in macro body", loc);
	mac.body = list(token.tlist.begin(), token.tlist.end());
	return true;
}
bool processNamespaceDef(string name, Loc loc, Loc percentLoc, Scope& scope) {
	Token token;
	directiveEatToken(Tlist, "Namespace body expected", false);
	scope.insertToken(Token(TInamespace, name, percentLoc, false, true));
	scope.currToken().tlist = token.tlist;
	scope.addInnerNamespace(name, percentLoc);
	return true;
}
bool processDirectiveDef(string directive, Scope& scope, Token percentToken, Loc loc) {
	static_assert(DefiningDirectivesCount == 3, "Exhaustive processDirectiveDef definition");
	string name; Token token;
	directiveEatIdentifier(directive, true, 1);
	if (directive == "define") {
		directiveEatToken(Tnumeric, "Define value expected", true);
		scope.currNamespace().defines[name] = Define(name, percentToken.loc, token.data);
	} else if (directive == "macro") {
		returnOnFalse(processMacroDef(scope, name, loc, percentToken.loc));
	} else if (directive == "namespace") {
		returnOnFalse(processNamespaceDef(name, loc, percentToken.loc, scope));
	} else {
		unreachable();
	}
	return check(!scope.hasNext() || scope.currToken().firstOnLine, "Unexpected token after directive", scope.currToken());
}
void expandDefineUse(Token percentToken, Scope& scope, int namespaceId, string defineName) {
	Define& define = IdToNamespace[namespaceId].defines[defineName];
	Token expanded = Token(Tnumeric, define.value, percentToken.loc, false, percentToken.firstOnLine);
	scope.insertToken(expanded);
}
bool preprocessImpl(Scope& scope);
bool processExpansionArglist(Token& token, Scope& scope, Macro& mac, Loc loc) {
	assert(token.type == Tlist);
	int count = 0;
	if (token.tlist.size()) {
		processArglistWrapper(
			bool retval = preprocessImpl(scope);
			retval &= scope.sliceArglist(mac, loc, count);
		);
	}
	return check(count == mac.argList.size(), "Unexpected number of expansion arguments", token.loc);
}
bool expandMacroUse(Loc loc, Scope& scope, int namespaceId, string macroName) {
	Macro& mac = IdToNamespace[namespaceId].macros[macroName]; Token token;
	directiveEatToken(Tlist, "Expansion arglist expected", true);
	returnOnFalse(processExpansionArglist(token, scope, mac, loc));
	checkReturnOnFail(!scope.hasNext() || scope.currToken().firstOnLine, "Unexpected token after macro use", scope.currToken());

	Token expanded = Token(TIexpansion, macroName, mac.loc, false, true);
	expanded.tlist = list(mac.body.begin(), mac.body.end());
	scope.addMacroExpansion(namespaceId, expanded);
	scope.insertToken(expanded);
	return true;
}
bool getDirectivePrefixes(string& firstName, list<string>& prefixes, list<Loc>& locs, Loc& loc, Scope& scope, bool& notContinued, string identPurpose="directive") {
	static_assert(TokenCount == 10, "Exhaustive getDirectivePrefixes definition");
	string name; bool first = true;
	while (true) {
		directiveEatIdentifier(identPurpose, false, 0);
		if (first) {
			firstName = name; first = false;
		} else prefixes.push_back(name);
		locs.push_back(loc);
		if (!scope.hasNext() || scope.currToken().type != Tcolon || scope.currToken().firstOnLine) break;
		loc = scope.eatenToken().loc;
	}
	notContinued = !scope.hasNext() || !scope.currToken().continued;
	return true;
}
bool defineDefined(string& name, int& namespaceId, bool firstPrefix=false) {
	if (IdToNamespace[namespaceId].defines.count(name)) return true;
	if (firstPrefix) for (int id : IdToNamespace[namespaceId].usedNamespaces) {
		if (IdToNamespace[id].defines.count(name)) {
			namespaceId = id;
			return true;
		}
	}
	return false;
}
bool macroDefined(string& name, int& namespaceId, bool firstPrefix=false) {
	if (IdToNamespace[namespaceId].macros.count(name)) return true;
	if (firstPrefix) for (int id : IdToNamespace[namespaceId].usedNamespaces) {
		if (IdToNamespace[id].macros.count(name)) {
			namespaceId = id;
			return true;
		}
	}
	return false;
}
bool namespaceDefined(string& name, int& namespaceId, bool firstPrefix=false) {
	if (IdToNamespace[namespaceId].innerNamespaces.count(name)) {
		namespaceId = IdToNamespace[namespaceId].innerNamespaces[name];
		return true;
	}
	if (firstPrefix) for (int id : IdToNamespace[namespaceId].usedNamespaces) {
		if (IdToNamespace[id].innerNamespaces.count(name)) {
			namespaceId = IdToNamespace[id].innerNamespaces[name];
			return true;
		}
	}
	return false;
}
void lookupFinalAbove(string directiveName, int& namespaceId, bool& namespaceSeen) {
	Namespace* currNamespace;
	while (true) {
		currNamespace = &IdToNamespace[namespaceId];
		if (defineDefined(directiveName, namespaceId, true) || macroDefined(directiveName, namespaceId, true)) return;
		namespaceSeen = namespaceSeen || currNamespace->innerNamespaces.count(directiveName);
		if (!currNamespace->isUpperAccesible) return;
		namespaceId = currNamespace->upperNamespaceId;
	}
}
bool lookupNamespaceAbove(string directiveName, int& namespaceId, Loc& loc) {
	Namespace* currNamespace;
	while (true) {
		currNamespace = &IdToNamespace[namespaceId];
		if (namespaceDefined(directiveName, namespaceId, true)) return true;
		if (!currNamespace->isUpperAccesible) break;
		namespaceId = currNamespace->upperNamespaceId;
	}
	return check(false, "Namespace not found" errorQuoted(directiveName), loc);
}
bool processUseDirective(string directiveName, Token& percentToken, Loc lastLoc, int namespaceId, bool notContinued, bool namespaceSeen, Scope& scope) {
	if (defineDefined(directiveName, namespaceId)) {
		checkReturnOnFail(notContinued, "Unexpected continued token", scope.currToken());
		expandDefineUse(percentToken, scope, namespaceId, directiveName);
		return true;
	} else if (macroDefined(directiveName, namespaceId)) {
		checkReturnOnFail(percentToken.firstOnLine && !scope.insideArglist(), "Unexpected macro use", percentToken.loc);
		return expandMacroUse(percentToken.loc, scope, namespaceId, directiveName);
	}
	checkReturnOnFail(!namespaceSeen && !namespaceDefined(directiveName, namespaceId, true), "Namespace used as directive" errorQuoted(directiveName), lastLoc);
	return check(false, "Undeclared identifier" errorQuoted(directiveName), lastLoc);
}
bool lookupName(Token& percentToken, string directiveName, list<string>& prefixes, list<Loc>& locs, bool notContinued, Scope& scope) {
	int namespaceId = scope.currNamespaceId(); bool namespaceSeen = false;
	if (prefixes.size()) {
		returnOnFalse(lookupNamespaceAbove(directiveName, namespaceId, locs.front()));
		directiveName = prefixes.front(); prefixes.pop_front(); locs.pop_front();
	} else {
		lookupFinalAbove(directiveName, namespaceId, namespaceSeen);
	}
	while (prefixes.size()) {
		checkReturnOnFail(namespaceDefined(directiveName, namespaceId), "Namespace not found" errorQuoted(directiveName), locs.front());
		directiveName = prefixes.front(); prefixes.pop_front(); locs.pop_front();
	}
	return processUseDirective(directiveName, percentToken, locs.front(), namespaceId, notContinued, namespaceSeen, scope);
}
bool processUsing(Loc loc, Scope& scope) {
	int namespaceId = scope.currNamespaceId();
	string firstName; list<string> prefixes; list<Loc> locs; bool notContinued;
	returnOnFalse(getDirectivePrefixes(firstName, prefixes, locs, loc, scope, notContinued, "namespace"));
	// NOTE the first namespace can be used, others down the using chain must be defined inside one another
	returnOnFalse(lookupNamespaceAbove(firstName, namespaceId, locs.front()));
	while (prefixes.size()) {
		firstName = prefixes.front(); prefixes.pop_front(); locs.pop_front();
		checkReturnOnFail(namespaceDefined(firstName, namespaceId), "Namespace not found" errorQuoted(firstName), locs.front());
	}
	check(scope.currNamespace().usedNamespaces.insert(namespaceId).second, "Namespace already imported" errorQuoted(firstName), locs.back());
	return true;
}
bool processBuiltinUse(string directive, Scope& scope, Loc directiveLoc) {
	static_assert(BuiltinDirectivesCount == 1, "Exhaustive processBuiltinUse definition");
	if (directive == "using") {
		returnOnFalse(processUsing(directiveLoc, scope));
	} else {
		unreachable();
	}
	return check(!scope.hasNext() || scope.currToken().firstOnLine, "Unexpected token after directive", scope.currToken());
}
#define processDirectiveSituationChecks(type) \
	checkReturnOnFail(!prefixes.size(), "Unexpected accessor", *(++locs.begin())); \
	checkReturnOnFail(notContinued, "Unexpected continued token", scope.currToken()); \
	if (type != "macro arg") { \
		checkReturnOnFail(!scope.insideArglist(), type " not allowed inside arglist", percentToken.loc); \
		checkReturnOnFail(!scope.insideMacro(), type " not allowed inside macro", percentToken.loc); \
		checkReturnOnFail(percentToken.firstOnLine, "Unexpected directive here" errorQuoted(directiveName), percentToken.loc); \
	}
bool processDirective(Token percentToken, Scope& scope) {
	static_assert(TokenCount == 10, "Exhaustive processDirective definition");
	string directiveName; list<string> prefixes; list<Loc> locs; Loc loc = percentToken.loc; bool notContinued;
	returnOnFalse(getDirectivePrefixes(directiveName, prefixes, locs, loc, scope, notContinued));
	if (DefiningDirectivesSet.count(directiveName)) {
		processDirectiveSituationChecks("Definition");
		returnOnFalse(processDirectiveDef(directiveName, scope, percentToken, loc));
	} else if (BuiltinDirectivesSet.count(directiveName)) {
		processDirectiveSituationChecks("Directive");
		returnOnFalse(processBuiltinUse(directiveName, scope, loc));
	} else if (!prefixes.size() && scope.hasMacroArg(directiveName)) {
		processDirectiveSituationChecks("macro arg");
		list<Token>& argField = scope.currMacro().nameToArg(directiveName).value.top();
		scope.insertList(argField, percentToken);
	} else {
		return lookupName(percentToken, directiveName, prefixes, locs, notContinued, scope);
	}
	return true;
}
#define eatLineOnFalse(cond) if (!(cond)) { scope.eatLine(); errorLess = false; continue; }
bool preprocessImpl(Scope& scope) {
	bool errorLess = true;
	while (scope.advanceIteration()) {
		Token& currToken = scope.currToken();
		if (currToken.type == Tspecial && currToken.data == "%") {
			eatLineOnFalse(check(!currToken.continued, "Directive must not continue", currToken.loc));
			eatLineOnFalse(processDirective(scope.eatenToken(), scope));
			continue;
		}
		scope.next(currToken);
	}
	return errorLess;
}
void preprocess(Scope& scope, string fileName) {
	IdToNamespace[NextNamespaceId] = Namespace(TOP_NAMESPACE_NAME, Loc(fileName, 0, 0), -1, false); NextNamespaceId++;
	preprocessImpl(scope);
}
// compilation -----------------------------------
bool eatComplexIdentifier(Scope& scope, Loc loc, string& ident, string purpose) {
	string fragment; Loc fragLoc = loc; bool canStartLine = purpose == "instr";
	checkReturnOnFail(scope.hasNext() && (!scope.currToken().firstOnLine || canStartLine), "Missing " + purpose + " name", loc);
	do {
		Token& token = scope.currToken();
		if (token.type == Tlist) {
			checkReturnOnFail(token.tlist.size(), "Expected " + purpose + " field", token.loc);
			processArglistWrapper(
				bool retval = eatIdentifier(scope, fragment, fragLoc, purpose + " field", canStartLine, 2);
				retval &= check(!scope.hasNext(), "Simple " + purpose + " field expected", token.loc);
			)
			ident.append(fragment);
			scope.next();
		} else {
			returnOnFalse(eatIdentifier(scope, fragment, token.loc, purpose + " field", canStartLine, 1));
			ident.append(fragment);
		}
	} while (scope.hasNext() && scope.currToken().continued);
	if (purpose == "instr") return true;
	returnOnFalse(isValidIdentifier(ident, "Invalid " + purpose + " name" errorQuoted(ident), loc));
	return checkIdentRedefinitions(ident, loc, true);
}
void initStrToLabel(list<Token>& tokens, string fileName) {
	StrToLabel = {{"begin", Label("begin", 0, Loc(fileName, 1, 1))}, {"end", Label("end", 0, Loc(fileName, 1, 1))}};
	if (tokens.size()) {
		StrToLabel["begin"].loc = tokens.front().loc;
		StrToLabel["end"].loc = tokens.back().loc;
	}
}
vector<Instr> compile(Scope& scope, string fileName) {
	vector<Instr> instrs;
	initStrToLabel(scope.currList(), fileName);
	Loc loc; bool errorLess, labelOnLine;
	while (scope.advanceIteration()) {
		Token& top = scope.currToken(); loc = top.loc;
		labelOnLine = labelOnLine && !top.firstOnLine;
		if (top.type == Tcolon) {
			string name;
			scope.next();
			eatLineOnFalse(eatComplexIdentifier(scope, loc, name, "label"));
			checkContinueOnFail(!labelOnLine, "Max one label per line", loc);
			labelOnLine = true;
			StrToLabel.insert(pair(name, Label(name, instrs.size(), loc)));
		} else if (top.type == Talpha) {
			string instrStr = "";
			eatLineOnFalse(eatComplexIdentifier(scope, loc, instrStr, "instr"));
			instrs.push_back(Instr(instrStr, loc));
			for (; scope.hasNext() && !scope.currToken().firstOnLine; scope.next()) {
				instrs.back().immFields.push_back(scope.currToken());
			}
		} else if (top.type == TIexpansion || top.type == TInamespace) {
			scope.next(top);
		} else {
			raiseError("Unexpected token", top);
			scope.eatLine();
		}
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

	Token module = Token(TImodule, TOP_NAMESPACE_NAME, Loc(fileName, 1, 1), false, true);
	ifstream inFile = getInputFile();
	tokenize(inFile, fileName, module.tlist);
	inFile.close();
	Scope scope(module);

	preprocess(scope, fileName);
	scope.init(module);
	vector<Instr> instrs = compile(scope, fileName);

	parseInstrs(instrs);
	raiseErrors();

	ofstream outFile = getOutputFile();
	generate(outFile, instrs);
	outFile.close();

	compileAndRun(OutputFileName);
}
