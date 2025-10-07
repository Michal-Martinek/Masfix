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
#include <optional>

#include <algorithm>
#include <numeric>
#include <math.h>
#include <assert.h>
#include <functional>
#include <cctype>
#include <locale>
#include <utility>
using namespace std;

// constants -------------------------------
#define TOP_MODULE_NAME "__main"

#define MAX_EXPANSION_DEPTH 1024

#define STDOUT_BUFF_SIZE 256
#define STDIN_BUFF_SIZE 256

#define WORD_MAX_VAL 65535
#define CELLS WORD_MAX_VAL+1
// enums --------------------------------
enum TokenTypes {
	Tnumeric,
	Talpha,
	Tstring,

	Tspecial,
	Tcolon,
	Tseparator,
	Tlist,

	// intermediate preprocess tokens
	TImodule,
	TIexpansion,
	TIctime,
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
constexpr int BuiltinDirectivesCount = 2;
const set<string> BuiltinDirectivesSet = {
	"using",
	"include"
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
	TokenTypes type=TokenCount;
	string data;
	list<Token> tlist;

	Loc loc;
	bool continued; // continues meaning of previous token
	bool firstOnLine;

	long long _ctimeFirstInstrIdx = -1;
	
	//–– construtors
    Token() = default;
    ~Token() = default; // TODO destructor?
	Token(TokenTypes type, string data, Loc loc, bool continued, bool firstOnLine) {
		this->type = type;
		this->data = data;
		this->loc = loc;
		this->continued = continued;
		this->firstOnLine = firstOnLine;
		_ctimeFirstInstrIdx = -1;
	}
	static Token fromCtx(TokenTypes type, string data, Token const& ctx) {
		return Token(type, move(data), ctx.loc, ctx.continued, ctx.firstOnLine);
	}
	//–– Copy
	Token(const Token& other)
		: type(other.type)
		, data(other.data)
		, tlist(other.tlist)
		, loc(other.loc)
		, continued(other.continued)
		, firstOnLine(other.firstOnLine)
		, _ctimeFirstInstrIdx(other._ctimeFirstInstrIdx)
	{}
	Token& operator=(const Token& other) {
		if (this != &other) {
			type = other.type;
			data = other.data;
			tlist = other.tlist;
			loc = other.loc;
			continued = other.continued;
			firstOnLine = other.firstOnLine;
			_ctimeFirstInstrIdx = other._ctimeFirstInstrIdx;
		}
		return *this;
	}
	//–– Move
	Token(Token&& other) noexcept
		: type(std::exchange(other.type, TokenCount))
		, data(std::move(other.data))
		, tlist(std::move(other.tlist))
		, loc(std::move(other.loc))
		, continued(other.continued)
		, firstOnLine(other.firstOnLine)
		, _ctimeFirstInstrIdx(std::exchange(other._ctimeFirstInstrIdx, -1))
	{}
	Token& operator=(Token&& other) noexcept {
	  if (this != &other) {
		type = std::exchange(other.type, TokenCount);
		data = std::move(other.data);
		tlist = std::move(other.tlist);
		loc = std::move(other.loc);
		continued = other.continued;
		firstOnLine = other.firstOnLine;
		_ctimeFirstInstrIdx = std::exchange(other._ctimeFirstInstrIdx, -1);
	  }
	  return *this;
	}

	char tlistCloseChar() {
		assert(type == Tlist);
		return data.at(0) + 2 - (data.at(0) == '(');
	}
	string toStr() {
		assert(this != nullptr);
		string out = data;
		if (type == Tlist) {
			if (isSeparated()) out.push_back(',');
			out.push_back(tlistCloseChar());
		}
		// } else if (type == Tstring && quotedStr) {
		// 	return '"' + data + '"';
		// }
		return out;
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
	stack<list<Token>> value;

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
	void addExpansionArgs(vector<pair<list<Token>::iterator, list<Token>::iterator>>& argSpans) {
		assert(argSpans.size() == argList.size());
		for (int idx = 0; idx < argSpans.size(); ++idx) {
			argList[idx].value.push(list<Token>(argSpans[idx].first, argSpans[idx].second));
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

	map<string, int> innerNamespaces;
	set<int> usedNamespaces;
	int upperNamespaceId;
	bool isUpperAccesible;

	Namespace() {}
	Namespace(string name, Loc loc, int upperNamespaceId, bool isUpperAccesible) {
		this->name = name;
		this->loc = loc;
		this->upperNamespaceId = upperNamespaceId;
		this->isUpperAccesible = isUpperAccesible;
	}
};

map<int, Namespace> IdToNamespace;

struct Module {
	fs::path abspath;
	Token contents; // TImodule
	int namespaceId;

	Module(fs::path path, int namespaceId) {
		abspath = path;
		this->namespaceId = namespaceId;
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
	vector<string> immFields;

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
		for (string s : immFields) {
			out.push_back(' ');
			out.append(s);
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
struct ParseCtx {
	vector<Instr> instrs;
	map<string, Label> strToLabel;
	Module* lastModule = nullptr;
	Token* parsedCtime; // ctime currently being parsed
	optional<ofstream> dumpFile;

	void close() {
		if (!!dumpFile) dumpFile->close();
	}
	/// saves idx of first instruction of ctime's body
	void saveCtimeStart(Token& ctime) {
		if (ctime._ctimeFirstInstrIdx == -1) {
			ctime._ctimeFirstInstrIdx = instrs.size();
		}
	}
	void removeCtimeInstrs(Token& ctime) {
		instrs.resize(ctime._ctimeFirstInstrIdx);
	}
};
ParseCtx parseCtx;
/// structure simulating the virtual machine during interpretation
struct VM {
	unsigned short head;
	unsigned short reg;
	unsigned short ip;
	unsigned short mem[CELLS];

	VM() {};
	void start(unsigned short startIdx) {
 		ip = startIdx;
	}
	unsigned short& cell() {
		return mem[head];
	}
};
VM globalVm;
/// holds all compile time flags and settings
struct Flags {
	bool verbose = false;
	bool run = false;
	bool keepAsm = false;
	bool dump = false;
	bool interpret = false;

	fs::path inputPath = "";
	vector<fs::path> includeFolders;

	fs::path filePath(string fileExt) {
		return inputPath.replace_extension(fileExt);
	}
	string filePathStr(string fileExt) {
		return '"' + filePath(fileExt).string() + '"';
	}
};
Flags flags;

// checks --------------------------------------------------------------------
#define errorQuoted(s) " '" + (s) + "'"
#define unreachable() assert(("Unreachable", false));

#define continueOnFalse(cond) if (!(cond)) { errorLess = false; continue; }
#define returnOnFalse(cond) if (!(cond)) return false;

#define check(cond, message, obj) ((cond) || raiseError(message, obj))
#define checkContinueOnFail(cond, message, obj) continueOnFalse(check(cond, message, obj));
#define checkReturnOnFail(cond, message, obj) returnOnFalse(check(cond, message, obj));

bool FLAG_strictErrors = false;
bool SupressErrors = false;
#define returnOnErrSupress() if (SupressErrors) return false;

vector<string> errors;
void raiseErrors() { // TODO sort errors based on line and file
	for (string s : errors) {
		cerr << s;
	}
	if (errors.size()) {
		exit(1);
	}
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
// struct Scope --------------------------------------------------------
void interpret(int startIdx=0);

/// responsible for iterating tokens and nested tlists,
/// keeping track of current module, namespace, expansion scope, arglist situation
struct Scope {
private:
	stack<int> namespaces;
	stack<pair<int, string>> macros;
	// TODO better max depth checks - maybe add depth counter
	stack<reference_wrapper<Token>> tlists;
	stack<list<Token>::iterator> itrs;
	list<Token> savedLine; // saving eaten tokens (from last instr) to restore when meeting ctime
	list<Module> modules;
	list<Module>::iterator currModule = modules.begin();
	bool isPreprocessing = true;

	/// opens new tlist for iteration
	void openList(Token& tlist) {
		tlists.push(tlist);
		itrs.push(tlist.tlist.begin());
	}
	/// handles the ending of a single token list
	/// forces parsing if apropriate
	void closeList() {
		static_assert(TokenCount == 12, "Exhaustive closeList definition");
		Token& closedList = tlists.top().get();
		tlists.pop(); itrs.pop();
		if (!isPreprocessing) return;
		if (closedList.type == TIexpansion) {
			endMacroExpansion();
		} else if (closedList.type == TInamespace) {
			assert(currNamespace().isUpperAccesible);
			exitNamespace();
		} else if (closedList.type == TImodule) {
			forceParse();
			exitNamespace();
			currModule++;
		} else if (closedList.type == TIctime) {
			parseInterpretCtime(closedList);
		}
	}

public:
	Scope() {}
	list<Token>& currList() {
		return tlists.top().get().tlist;
	}
	Token& currToken() {
		return *itrs.top();
	}
	Token* operator->() {
		return &currToken();
	}
	/// is current token list of the specified type
	bool isCurrListType(TokenTypes type) {
		return tlists.top().get().type == type;
	}
	int expansionDepth() {
		return max(0, (int)tlists.size() - 2);
	}

// iteration ----------------------------------------------------------

	/// whether currList has any next token
	bool hasNext() {
		return itrs.top() != currList().end();
	}
	/// closes ended lists if necessary;
	/// generally closes only lists opened in an ordinary way, the special ones will be closed elsewhere;
	/// flushes eaten tokens for every line start (in both modes)
	/// @returns true - guarantees state ready for further processing
	bool advanceIteration() {
		while (tlists.size() && !hasNext() && !isCurrListType(TIarglist) && (isPreprocessing || !isCurrListType(TImodule))) {
			if (!isPreprocessing && isCurrListType(TIctime)) return false; // escape after ctime's body was parsed
			closeList();
		}
		returnOnFalse(tlists.size() && hasNext());
		if (currToken().firstOnLine) flushSavedLine(); // save only current instruction
		// escape when meeting ctime (while parsing module)
		if (!isPreprocessing && currToken().type == TIctime) {
			return handleCtimeMeet(currToken());
		}
		return tlists.size() && hasNext();
	}
	/// @brief properly advances parsing when meeting ctime during module parsing
	/// @return continue parsing?
	bool handleCtimeMeet(Token& ctime) {
		assert(parseCtx.parsedCtime != nullptr);
		parseCtx.saveCtimeStart(ctime);
		restoreSavedLine();
		return _advanceScopesCtimeMeet(ctime);
	}
	bool _advanceScopesCtimeMeet(Token& ctime) {
		if (&ctime != parseCtx.parsedCtime) {
			next(ctime); // advance into ctime which is not parsed now
			return advanceIteration();
		}
		// parsedCtime met while parsing module -> exit -> forceParse(parsedCtime)
		while (tlists.size() && !isCurrListType(TImodule)) {
			closeList();
		}
		if (tlists.size()) itrs.top() = currList().end(); // guarantee parsing exit
		return false;
	}
	/// advances iteration inside current list
	list<Token>::iterator& next() {
		return ++itrs.top();
	}
	/// advances iteration, opens new nested list if provided
	list<Token>::iterator& next(Token& tlist) {
		static_assert(TokenCount == 12, "Exhaustive Scope::next definition");
		++itrs.top();
		if (tlist.type == Tlist || tlist.type == TIexpansion || tlist.type == TInamespace || tlist.type == TIctime) {
			openList(tlist);
		}
		return itrs.top();
	}
// token stream manipulation ------------------------------------------
	void insertToken(Token&& token) {
		itrs.top() = currList().insert(itrs.top(), move(token));
	}
	/// moves Tokens / inserts copies of Tokens from tlist into currList
	/// first inserted inherits context from percentToken
	void insertList(list<Token>& tlist, Token& percentToken, bool copy) {
		assert(tlist.size());
		if (copy) {
			itrs.top() = currList().insert(itrs.top(), tlist.begin(), tlist.end());
		} else {
			list<Token>::iterator inserted = tlist.begin();
			currList().splice(itrs.top(), tlist);
			itrs.top() = inserted;
		}
		itrs.top()->continued = percentToken.continued;
		itrs.top()->firstOnLine = percentToken.firstOnLine;
	}
	/// eats token from token stream and returns it
	Token& eatenToken(bool skipNoEat=false) {
		auto nextNode = std::next(itrs.top());
		if (!skipNoEat) savedLine.splice(savedLine.end(), currList(), itrs.top());
		itrs.top() = nextNode;
		return skipNoEat ? *prev(itrs.top()) : savedLine.back();
	}
	/// eats tokens upto EOL or separator
	/// ignores ctime
	void eatLine(bool surelyEat=false) {
		while (hasNext() && (surelyEat || !currToken().firstOnLine) && !(isCurrListType(TIarglist) && currToken().type == Tseparator) && currToken().type != TIctime) {
			eatenToken();
			surelyEat = false;
		}
	}
	/// restores the eaten line into TS, iteration then skips it
	/// used because instruction is not ready yet due to pending ctime expansion
	void restoreSavedLine() {
		currList().splice(itrs.top(), savedLine);
	}
	void flushSavedLine() {
		savedLine.clear();
	}
// situation checks -----------------------------------------
	bool insideMacro() {
		return macros.size();
	}
	int currNamespaceId() {
		assert(namespaces.size());
		return insideMacro() ? macros.top().first : namespaces.top();
	}
	Namespace& currNamespace() {
		return IdToNamespace[currNamespaceId()];
	}
	Macro& currMacro() {
		assert(insideMacro());
		return currNamespace().macros[macros.top().second];
	}
	fs::path currModuleFolder() {
		return currModule->abspath.parent_path();
	}
	bool hasMacroArg(string& name) {
		return insideMacro() && currMacro().hasArg(name);
	}
	void addMacroExpansion(int namespaceId, Token& expansionToken) {
		if (macros.size() > MAX_EXPANSION_DEPTH) { // TODO?
			raiseError("Maximum expansion depth exceeded", expansionToken.loc, true);
		}
		macros.push(pair(namespaceId, expansionToken.data));
	}
	void endMacroExpansion() {
		currMacro().closeExpansionScope();
		macros.pop();
	}
	int addNewNamespace(string& name, Loc loc, bool isModuleDefinition) {
		assert(!insideMacro());
		int newId = IdToNamespace.size();
		if (newId != 0) {
			if (isModuleDefinition) currNamespace().usedNamespaces.insert(newId);
			else currNamespace().innerNamespaces[name] = newId;
		}
		IdToNamespace[newId] = Namespace(name, loc, isModuleDefinition ? -1 : currNamespaceId(), !isModuleDefinition);
		namespaces.push(newId);
		return newId;
	}
	void exitNamespace() {
		assert(namespaces.size() >= 1);
		namespaces.pop();
	}
	void enterArglist(Token& tlist, bool changeType=true) {
		if (changeType) tlist.type = TIarglist;
		openList(tlist);
	}
	/// @brief closes curr tlist
	/// @param allowAnyType allows any tlist type
	void exitArglist(bool allowAnyType=false) {
		if (!allowAnyType) assert(isCurrListType(TIarglist));
		closeList();
	}
// tokenization -----------------------------------------
	bool tokenizeHasTlist() {
		return tlists.size() && isCurrListType(Tlist);
	}
	bool tokenizeCloseList(char closeChar, Loc& loc) {
		checkReturnOnFail(tokenizeHasTlist(), "Unexpected token list termination" errorQuoted(string(1, closeChar)), loc);
		checkReturnOnFail(closeChar == tlists.top().get().tlistCloseChar(), "Unmatched token list delimiters" errorQuoted(string(1, closeChar)), loc);
		closeList();
		return true;
	}
	void tokenizeEnd() {
		while (tokenizeHasTlist()) {
			raiseError("Unclosed token list", tlists.top().get());
			closeList();
		}
		assert(tlists.size() && isCurrListType(TImodule));
		itrs.top() = currList().begin();
	}
// modules -------------------------------------------------
	Module* getCurrModule() {
		return &*currModule;
	}
	bool newModuleIncluded(fs::path abspath) {
		for (list<Module>::iterator module = modules.begin(); module != modules.end(); ++module) {
			if (fs::equivalent(module->abspath, abspath)) {
				currNamespace().usedNamespaces.insert(module->namespaceId);
				return false;
			}
		}
		return true;
	}
	void addNewModule(fs::path abspath, string relPath, string moduleName) {
		Loc loc = Loc(relPath, 1, 1);
		int namespaceId = addNewNamespace(moduleName, loc, true);
		currModule = modules.insert(currModule, Module(abspath, namespaceId));
		currModule->contents = Token(TImodule, moduleName, loc, false, true);
		openList(currModule->contents);
	}
	bool forceParseImpl(Token* ctime, bool final=true);
	/// Scope wrapper for parsing, cleans Scope after parsing
	/// parses whole TImodule (upto possible ctime, then it's body)
	/// returns if instrs are ready to execute
	bool forceParse(Token* ctime=nullptr) {
		assert(isPreprocessing);
		isPreprocessing = false;
		int numTlistsBefore = tlists.size();
		bool safeToRun=true;
		parseCtx.parsedCtime = ctime;
		/// NOTE all tlists are opened again specifically for parsing and discarded afterwards
		openList(currModule->contents);
		forceParseImpl(ctime, ctime == nullptr);
		if (ctime) {
			/* assert */(ctime->_ctimeFirstInstrIdx != -1) || raiseError("CRITICAL ctime used in unexpected context", *ctime, true);
			openList(*ctime);
			safeToRun = forceParseImpl(ctime);
			assert(isCurrListType(TIctime) && &currList() == &ctime->tlist);
			closeList();
		}
		assert(isCurrListType(TImodule) && &tlists.top().get() == &currModule->contents);
		closeList();
		assert(tlists.size() == numTlistsBefore);
		isPreprocessing = true;
		return safeToRun;
	}
	/// handles ctime use after it's body has been preprocessed
	/// parses everything needed, runs the VM, handles ctime's return value(s)
	void parseInterpretCtime(Token& ctimeExp) {
		bool safeToRun = forceParse(&ctimeExp);
		string retval="0";
		if (safeToRun) {
			interpret(ctimeExp._ctimeFirstInstrIdx);
			retval = to_string(globalVm.reg);
		}

		parseCtx.removeCtimeInstrs(ctimeExp);
		Token retValToken = Token::fromCtx(Tnumeric, retval, ctimeExp);
		assert(&*--itrs.top() == &ctimeExp);
		eatenToken();
		flushSavedLine();
		insertToken(move(retValToken));
		endMacroExpansion();
	}

// helpers -------------------------------------------------
	bool _addMacroArg(vector<pair<list<Token>::iterator, list<Token>::iterator>>& argSpans, list<Token>::iterator& firstArg, Loc loc) {
		checkReturnOnFail(firstArg != itrs.top(), "Argument expected", loc);
		argSpans.push_back(pair(firstArg, itrs.top()));
		return true;
	}
	bool sliceArglist(Macro& mac, Loc loc, bool retval=true) {
		itrs.top() = currList().begin();
		list<Token>::iterator argStart = --currList().begin(); // points before the starting element, so as not to get invalidated
		vector<pair<list<Token>::iterator, list<Token>::iterator>> argSpans;
		while (hasNext()) {
			loc = currToken().loc;
			checkReturnOnFail(mac.argList.size(), "Excesive expansion argument", loc);
			if (currToken().type == Tseparator) {
				checkReturnOnFail(argSpans.size()+1 < mac.argList.size(), "Excesive expansion argument", loc);
				retval &= _addMacroArg(argSpans, ++argStart, loc);
				argStart = itrs.top();
			} else if (currToken().type == TIexpansion) {
				Token& exp = eatenToken();
				if (exp.tlist.size()) insertList(exp.tlist, exp, false);
				continue;
			}
			next();
		}
		retval &= _addMacroArg(argSpans, ++argStart, loc);
		returnOnFalse(retval && check(argSpans.size() == mac.argList.size(), "Missing expansion arguments", loc));
		mac.addExpansionArgs(argSpans);
		return true;
	}
};

// tokenization -----------------------------------------------------------------
string getCharRun(string s) {
	bool num = isdigit(s.at(0)), alpha = isalpha(s.at(0)), space = isspace(s.at(0));
	if (!num && !alpha && !space) return string(1, s.at(0));
	string out;
	int i = 0;
	do {
		out.push_back(s.at(i));
		i++;
	} while (i < s.size() && !!isdigit(s.at(i)) == num && !!isalpha(s.at(i)) == alpha && !!isspace(s.at(i)) == space);
	return out;
}
#define addToken(type) scope.insertToken(Token(type, run, loc, continued, firstOnLine)); \
					scope.next(scope.currToken());
/// reads file, performs lexical analysis, builds token stream
/// prepares Scope for preprocessing
void tokenize(ifstream& ifs, string relPath, Scope& scope) {
	static_assert(TokenCount == 12, "Exhaustive tokenize definition");
	string line;
	bool continued, firstOnLine, keepContinued;
	for (int lineNum = 1; getline(ifs, line); ++lineNum) {
		continued = false; firstOnLine = true;
		line = line.substr(0, line.find(';'));
		int col = 1;
		while (line.size()) {
			char first = line.at(0);
			string run = getCharRun(line);
			line = line.substr(run.size());

			Loc loc = Loc(relPath, lineNum, col);
			col += run.size();
			keepContinued = true;
			if (isspace(first)) {
				continued = false;
				continue;
			}
			if (isdigit(first)) {
				addToken(Tnumeric);
			} else if (isalpha(first)) {
				addToken(Talpha);
			} else if (run.size() == 1) {
				if (first == '(' || first == '[' || first == '{') {
					addToken(Tlist);
					keepContinued = false;
				} else if (first == ')' || first == ']' || first == '}') {
					if (!scope.tokenizeCloseList(first, loc)) continue;
				} else if (first == ':') {
					addToken(Tcolon);
				} else if (first == ',') {
					continued = false;
					addToken(Tseparator);
					keepContinued = false;
				} else if (first == '"') {
					size_t quotePos = line.find('"');
					if (!check(quotePos != string::npos, "Expected string termination", loc)) {
						line = ""; continue;
					}
					run = line.substr(0, quotePos);
					col +=++ quotePos;
					continued = false; keepContinued = false;
					addToken(Tstring);
					line = line.substr(quotePos);
				} else {
					addToken(Tspecial);
				}
			} else {
				assert(false);
			}
			continued = keepContinued;
			firstOnLine = false;
		}
	}
	scope.tokenizeEnd();
}
// preprocess helpers -------------------------------------------------------------------------
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
		checkReturnOnFail(parseCtx.strToLabel.count(name) == 0, "Label redefinition" errorQuoted(name), loc);
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
	body; \
	scope.exitArglist(); \
	returnOnFalse(retval);
bool eatToken(Scope& scope, Loc& loc, Token& outToken, TokenTypes type, string errMissing, bool sameLine) {
	checkReturnOnFail(scope.hasNext(), errMissing, loc);
	if (sameLine) checkReturnOnFail(!scope->firstOnLine, errMissing, loc);
	outToken = scope.eatenToken(); loc = outToken.loc;
	// TODO check before eating?
	return check(outToken.type == type, "Unexpected token type", outToken); // TODO more specific err message here
}
void _eatTokenRun(Scope& scope, string& name, Loc& loc, bool canStartLine=true, int eatAnything=0, bool skipNoEat=false) {
	static_assert(TokenCount == 12, "Exhaustive _eatTokenRun definition");
	if (scope.hasNext()) loc = scope->loc;
	bool first = true; name = "";

	set<TokenTypes> allowedTypes = {Tnumeric, Talpha, Tspecial};
	if (eatAnything >= 1) allowedTypes.insert(Tcolon);
	if (eatAnything >= 2) allowedTypes.insert(Tstring);
	if (eatAnything >= 3) allowedTypes.insert(Tlist);
	while (scope.hasNext() && (!scope->firstOnLine || (canStartLine && first)) && (first || scope->continued) && allowedTypes.count(scope->type)) {
		name += scope.eatenToken(skipNoEat).toStr();
		first = false;
	}
}
/// @brief joins tokens in TS as identifier (name) given naming policies
/// @param name output ref - identifier
/// @param loc in - loc of prev token for errs, out - loc of ident start
/// @param identPurpose description for errors
/// @param canStartLine is identifier expected to follow prev token or start on newline
/// @param eatAnything the higher level - more token types are allowed: 0 - basics, 1 - :, 2 - "", 3 - []
/// @param skipNoEat skips tokens instead of eating them
/// @return if identifier was successfully read (false if ctime interrupted eating)
bool eatIdentifier(Scope& scope, string& name, Loc& loc, string identPurpose, bool canStartLine=false, int eatAnything=0, bool skipNoEat=false) {
	Loc prevLoc = loc;
	_eatTokenRun(scope, name, loc, canStartLine, eatAnything, skipNoEat);
	// break if ctime was seen first, otherwise it's not part of ident because it's never continued
	if (scope.hasNext() && scope->type == TIctime) return name != "";
	return check(name != "", "Missing " + identPurpose + " name", prevLoc);
}
#define directiveEatIdentifier(identPurpose, definition, eatAnything) \
	returnOnFalse(eatIdentifier(scope, name, loc, identPurpose, false, eatAnything)); \
	returnOnFalse(isValidIdentifier(name, "Invalid " + string(identPurpose) + " name" errorQuoted(name), loc)); \
	if (definition) returnOnFalse(checkIdentRedefinitions(name, loc, false, &scope.currNamespace()));
#define directiveEatToken(type, missingErr, sameLine) returnOnFalse(eatToken(scope, loc, token, type, missingErr, sameLine));

string relPathFromMasfix(fs::path p) {
	fs::path out;
	bool found = false;
	for (fs::path part : p) {
		string s = part.string();
		if (found) out.append(s);
		if (s == "Masfix") {
			found = true;
		}
	}
	return (found ? out : p).string();
}
ifstream openInputFile(fs::path path);
string tokenizeNewModule(fs::path abspath, Scope& scope, bool mainModule=false) {
	string relPath = relPathFromMasfix(abspath);
	string moduleName = mainModule ? TOP_MODULE_NAME : abspath.filename().replace_extension("").string(); // TODO name sanitazion, module name redefs?
	scope.addNewModule(abspath, relPath, moduleName);
	ifstream ifs = openInputFile(abspath);
	tokenize(ifs, relPath, scope);
	ifs.close();
	return relPath;
}

// preprocess -------------------------------------------------------------------------
bool arglistFromTlist(Scope& scope, Loc& loc, Macro& mac) {
	string name; bool first = true;
	while (scope.hasNext()) {
		if (!first) {
			const Token& sep = scope.eatenToken(); loc = sep.loc;
			checkReturnOnFail(sep.type == Tseparator, "Separator expected", loc);
		}
		first = false;
		directiveEatIdentifier("macro arg", true, 1);
		checkReturnOnFail(mac.nameToArgIdx.count(name) == 0, "Macro argument redefinition" errorQuoted(name), loc);
		mac.addArg(name, loc); // TODO macArg.loc not checked nor used
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
bool processNamespaceDef(string name, Loc loc, Token& percentToken, Scope& scope) {
	Token token; // TODO no seps in namespace body
	directiveEatToken(Tlist, "Namespace body expected", false);
	scope.insertToken(Token::fromCtx(TInamespace, name, percentToken));
	scope->tlist = token.tlist;
	scope.addNewNamespace(name, percentToken.loc, false);
	return true;
}
bool processDirectiveDef(string directive, Scope& scope, Token& percentToken, Loc loc) {
	static_assert(DefiningDirectivesCount == 3, "Exhaustive processDirectiveDef definition");
	string name; Token token;
	directiveEatIdentifier(directive, true, 1);
	if (directive == "define") {
		directiveEatToken(Tnumeric, "Define value expected", true);
		scope.currNamespace().defines[name] = Define(name, percentToken.loc, token.data);
	} else if (directive == "macro") {
		returnOnFalse(processMacroDef(scope, name, loc, percentToken.loc));
	} else if (directive == "namespace") {
		returnOnFalse(processNamespaceDef(name, loc, percentToken, scope));
	} else {
		unreachable();
	}
	return check(!scope.hasNext() || scope->firstOnLine, "Unexpected token after directive", scope.currToken());
}
void expandDefineUse(Token& percentToken, Scope& scope, int namespaceId, string defineName) {
	Define& define = IdToNamespace[namespaceId].defines[defineName];
	scope.insertToken(Token::fromCtx(Tnumeric, define.value, percentToken));
}
bool preprocess(Scope& scope);
bool processExpansionArglist(Token& token, Scope& scope, Macro& mac, Loc& loc) {
	assert(token.type == Tlist);
	if (!token.tlist.size()) return check(mac.argList.size() == 0, "Missing expansion arguments", loc);
	processArglistWrapper(
		bool retval = preprocess(scope);
		retval = retval && scope.sliceArglist(mac, loc);
	);
	return true;
}
bool expandMacroUse(Scope& scope, int namespaceId, string macroName, Token& percentToken) {
	bool ctime = percentToken.data == "!";
	Macro& mac = IdToNamespace[namespaceId].macros[macroName]; Token token; Loc loc = percentToken.loc;
	directiveEatToken(Tlist, "Expansion arglist expected", true);
	returnOnFalse(processExpansionArglist(token, scope, mac, loc));
	checkReturnOnFail(!scope.hasNext() || scope->firstOnLine || (ctime && !scope->continued), "Unexpected token after macro use", scope.currToken());

	Token expanded = Token::fromCtx(ctime ? TIctime : TIexpansion, macroName, percentToken);
	expanded.tlist = list(mac.body.begin(), mac.body.end());
	scope.addMacroExpansion(namespaceId, expanded);
	scope.insertToken(move(expanded));
	return true;
}
bool getDirectivePrefixes(string& firstName, list<string>& prefixes, list<Loc>& locs, Loc& loc, Scope& scope, bool& notContinued, string identPurpose="directive") {
	static_assert(TokenCount == 12, "Exhaustive getDirectivePrefixes definition");
	string name; bool first = true;
	while (true) {
		directiveEatIdentifier(identPurpose, false, 0);
		if (first) {
			firstName = name; first = false;
		} else prefixes.push_back(name);
		locs.push_back(loc);
		if (!scope.hasNext() || scope->type != Tcolon || scope->firstOnLine) break;
		loc = scope.eatenToken().loc;
	}
	notContinued = !scope.hasNext() || !scope->continued;
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
		checkReturnOnFail(percentToken.firstOnLine || (percentToken.data == "!" && !percentToken.continued), "Unexpected macro use", percentToken.loc);
		return expandMacroUse(scope, namespaceId, directiveName, percentToken);
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
fs::path processIncludePath(string str, Scope& scope) {
	fs::path path = fs::path(str);
	if (path.has_extension() && path.extension() != ".mx") return fs::path();
	fs::path moduleRel = scope.currModuleFolder() / path.replace_extension(".mx");
	if (fs::exists(moduleRel)) return fs::canonical(moduleRel);

	for (fs::path prepath : flags.includeFolders) {
		path = prepath;
		path /= fs::path(str).replace_extension(".mx");
		if (fs::exists(path)) break;
	}
	return fs::exists(path) ? fs::canonical(path) : fs::path();
}
bool processBuiltinUse(string directive, Scope& scope, Loc loc) {
	static_assert(BuiltinDirectivesCount == 2, "Exhaustive processBuiltinUse definition");
	Token token;
	if (directive == "using") {
		returnOnFalse(processUsing(loc, scope));
	} else if (directive == "include") {
		directiveEatToken(Tstring, "Missing included path string", true);
		fs::path path = processIncludePath(token.data, scope);
		checkReturnOnFail(fs::exists(path), "Input file \"" + token.data + "\" can't be included", loc);
		if (scope.newModuleIncluded(path)) tokenizeNewModule(path, scope);
		// TODO unexpected token after include
	} else {
		unreachable();
	}
	return check(!scope.hasNext() || scope->firstOnLine, "Unexpected token after directive", scope.currToken());
}
#define processDirectiveSituationChecks(type) \
	checkReturnOnFail(!prefixes.size(), "Unexpected accessor", *(++locs.begin())); \
	checkReturnOnFail(percentToken.data != "!", "Unexpected ctime forcing", percentToken); \
	checkReturnOnFail(notContinued, "Unexpected continued token", scope.currToken()); \
	if (type != "macro arg") { \
		checkReturnOnFail(!scope.isCurrListType(TIarglist), type " not allowed inside arglist", percentToken.loc); \
		checkReturnOnFail(!scope.insideMacro(), type " not allowed inside macro", percentToken.loc); \
		checkReturnOnFail(percentToken.firstOnLine, "Unexpected directive here" errorQuoted(directiveName), percentToken.loc); \
	}
bool processDirective(Token percentToken, Scope& scope) {
	static_assert(TokenCount == 12, "Exhaustive processDirective definition");
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
		scope.insertList(argField, percentToken, true);
	} else {
		return lookupName(percentToken, directiveName, prefixes, locs, notContinued, scope);
	}
	return true;
}
#define eatLineOnFalse(cond) if (!(cond)) { \
	scope.eatLine(); \
	errorLess = false; \
	continue; \
	}
/// preprocesses given scope, parses completed modules
/// closes all scopes
bool preprocess(Scope& scope) {
	bool errorLess = true;
	while (scope.advanceIteration()) {
		Token& currToken = scope.currToken();
		if (currToken.type == Tspecial && (currToken.data == "%" || currToken.data == "!")) {
			eatLineOnFalse(check(!currToken.continued, "Directive must not continue", currToken.loc));
			eatLineOnFalse(processDirective(scope.eatenToken(), scope));
			continue;
		}
		scope.next(currToken);
	}
	return errorLess;
}
// token stream parsing -----------------------------------
bool eatComplexIdentifier(Scope& scope, Loc loc, string& ident, string purpose) {
	string fragment; Loc fragLoc = loc; bool canStartLine = purpose == "instr";
	checkReturnOnFail(scope.hasNext() && (!scope->firstOnLine || canStartLine), "Missing " + purpose + " name", loc);
	do {
		Token& token = scope.currToken();
		if (token.type == Tlist) {
			fragLoc = token.loc;
			checkReturnOnFail(token.tlist.size(), "Expected " + purpose + " field", fragLoc);
			scope.enterArglist(scope.eatenToken(), false);
				bool retval = eatIdentifier(scope, fragment, fragLoc, purpose + " field", false, 2, true);
				Token* nextToken=nullptr; if (scope.hasNext()) nextToken = &scope.currToken();
			scope.exitArglist(true);
			if (!!nextToken && nextToken->type == TIctime) {
				scope.handleCtimeMeet(*nextToken);
				return false;
			}
			returnOnFalse(retval);
			checkReturnOnFail(!nextToken, "Simple " + purpose + " field expected", *nextToken);
			ident.append(fragment);
		} else {
			returnOnFalse(eatIdentifier(scope, fragment, fragLoc, purpose + " field", canStartLine, 2));
			canStartLine = false;
			ident.append(fragment);
		}
	} while (scope.hasNext() && scope->continued);
	if (purpose == "instr" || purpose == "immediate") return true;
	returnOnFalse(isValidIdentifier(ident, "Invalid " + purpose + " name" errorQuoted(ident), loc));
	return checkIdentRedefinitions(ident, loc, true);
}

bool dumpImpl(optional<ofstream>& dumpFile, int indent, string s) {
	// TODO disable in ctime
	dumpFile.value() << string(indent, '\t') << s << '\n';
	return true;
}
#define dump(str) (!!parseCtx.dumpFile) && dumpImpl(parseCtx.dumpFile, scope.expansionDepth(), str)
#define dumpExpansion(str) dump("; " + str)
/// parses suplied token stream from tokens to instruction fields
/// parses upto EOF or possible TIctime
/// leaves intermediate Tlists in token stream (TIexpansion, TInamespace, parsed TImodule)
bool parseTokenStream(Scope& scope) {
	Loc loc;
	bool labelOnLine = false, errorLess = true;
outer_loop_continue:
	while (scope.advanceIteration()) {
		if (scope.getCurrModule() != parseCtx.lastModule) {
			parseCtx.lastModule = scope.getCurrModule();
			dumpExpansion(parseCtx.lastModule->abspath.string() + " ----");
		}
		Token& top = scope.currToken(); loc = top.loc; string name;
		labelOnLine = labelOnLine && !top.firstOnLine;
		if (top.type == Tcolon) {
			scope.eatenToken();
			eatLineOnFalse(eatComplexIdentifier(scope, loc, name, "label"));
			checkContinueOnFail(!labelOnLine, "Max one label per line", loc);
			labelOnLine = true;
			parseCtx.strToLabel.insert(pair(name, Label(name, parseCtx.instrs.size(), loc)));
			scope.flushSavedLine(); // label and instr can be independently on same line
			dump(':' + name);
		} else if (top.type == Talpha) {
			eatLineOnFalse(eatComplexIdentifier(scope, loc, name, "instr"));
			Instr instr(name, loc);
			while (scope.hasNext() && !scope->firstOnLine) {
				string immFrag;
				// NOTE eatLine on false + continue outer loop
				if (!eatComplexIdentifier(scope, loc, immFrag, "immediate")) {
					scope.eatLine();
					errorLess = false;
					goto outer_loop_continue;
				}
				instr.immFields.push_back(immFrag);
			}
			parseCtx.instrs.push_back(instr);
			dump(instr.toStr());
		} else if (top.type == TIexpansion || top.type == TInamespace) {
			dumpExpansion(top.loc.toStr() + ' ' + top.data);
			scope.next(top);
		} else {
			raiseError("Unexpected token", top);
			errorLess = false;
			scope.eatLine(true);
		}
	}
	return errorLess;
}
// parsing instruction fields -------------------------------------------------------------------
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
bool parseInstrImmediate(Instr& instr, bool maybeUndefined) {
	checkReturnOnFail(instr.immFields.size() == 1, "Only simple immediates for now", instr);
	string immVal = instr.immFields[0];
	if (parseCtx.strToLabel.count(immVal) > 0) {
		instr.immediate = parseCtx.strToLabel[immVal].addr;
		return true;
	}
	if (maybeUndefined && !isdigit(immVal.at(0))) 
	return false;
	checkReturnOnFail(isdigit(immVal.at(0)), "Invalid instruction immediate", instr);
	instr.immediate = stoi(immVal);
	checkReturnOnFail(to_string(instr.immediate) == immVal, "Invalid instruction immediate", instr);
	return check(0 <= instr.immediate && instr.immediate <= WORD_MAX_VAL, "Value of the immediate is out of bounds", instr);
}
bool parseInstrFields(Instr& instr, bool final) {
	returnOnFalse(parseInstrOpcode(instr));
	if (instr.hasImm()) {
		returnOnFalse(parseInstrImmediate(instr, !final));
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
/// @brief parses instructions and immediates for their meaning
/// @param final if false silently ignores use of undefined labels
/// @return if instrs are ready to be executed
bool parseInstrs(vector<Instr>& instrs, int startIdx, bool final) {
	bool errorLess = true;
	for (size_t i = startIdx; i < instrs.size(); ++i) {
		Instr& instr = instrs[i];
		bool good = parseInstrFields(instr, final);
		good = good && checkValidity(instr);
		if (!good) instr.instr = InstructionCount; // ensure noone tries executing it
		errorLess &= good;
	}
	check(instrs.size() <= WORD_MAX_VAL+1, "The instruction count " + to_string(instrs.size()) + " exceeds WORD_MAX_VAL=" + to_string(WORD_MAX_VAL), instrs[instrs.size()-1].opcodeLoc);
	return errorLess;
}
bool Scope::forceParseImpl(Token* ctime, bool final) {
	size_t parseStart = 0;
	if (ctime) parseStart = final ? ctime->_ctimeFirstInstrIdx : parseCtx.instrs.size();
	bool ret = parseTokenStream(*this);
	parseCtx.strToLabel["end"].addr = parseCtx.instrs.size();
	return parseInstrs(parseCtx.instrs, parseStart, final) && ret;
}
// interpreting -------------------------------------------------
unsigned short interpGetReg(VM& vm, RegNames reg) {
	static_assert(RegisterCount == 5, "Exhaustive interpGetReg definition");
	if (reg == Rh) return vm.head;
	else if (reg == Rm) return vm.cell();
	else if (reg == Rr) return vm.reg;
	else if (reg == Rp) return vm.ip;
	else unreachable();
}
unsigned short interpOperation(VM& vm, OpNames op, unsigned short left, unsigned short right) {
	static_assert(OperationCount == 10, "Exhaustive interpOperation definition");
	if (op == OPa) return left + right;
	else if (op == OPs) return left - right;
	else if (op == OPt) return left * right;
	else if (op == OPand) return left & right;
	else if (op == OPor) return left | right;
	else if (op == OPxor) return left ^ right;
	else if (op == OPshl) return left << right;
	else if (op == OPshr) return left >> right;
	else if (op == OPbit) return !!(left & (1 << right));
	else unreachable();
}
bool interpCond(VM& vm, Instr& instr, signed short target) {
	static_assert(ConditionCount == 7, "Exhaustive interpCond definition");
	short reg = (signed short) interpGetReg(vm, instr.suffixes.condReg);
	if (instr.instr == Ib) target = 0;

	if (instr.suffixes.cond == Ceq) return reg == target;
	if (instr.suffixes.cond == Cne) return reg != target;
	if (instr.suffixes.cond == Clt) return reg <  target;
	if (instr.suffixes.cond == Cle) return reg <= target;
	if (instr.suffixes.cond == Cgt) return reg >  target;
	if (instr.suffixes.cond == Cge) return reg >= target;
	unreachable();
}
void interpInstrBody(VM& vm, Instr& instr, unsigned short target, bool cond, bool& ipChanged) {
	static_assert(InstructionCount == 14, "Exhaustive interpInstrBody definition");
	unsigned short& inputReg = instr.suffixes.reg == Rm ? vm.cell() : vm.reg;
	if (instr.instr == Imov) vm.head = target;
	else if (instr.instr == Istr) {
		vm.cell() = target;
	} else if (instr.instr == Ild) {
		vm.reg = target;
	} else if (instr.instr == Ijmp) {
		vm.ip = target;
		ipChanged = true;
	} else if (instr.instr == Ib) {
		if (cond) {
			vm.ip = target;
			ipChanged = true;
		}
	} else if (instr.instr == Il) {
		vm.reg = cond ? 1 : 0;
	} else if (instr.instr == Is) {
		vm.cell() = cond ? 1 : 0;
	} else if (instr.instr == Iswap) {
		unsigned short temp = vm.cell();
		vm.cell() = vm.reg;
		vm.reg = temp;
	} else if (instr.instr == Ioutu) {
		cout << target;
	} else if (instr.instr == Ioutc) {
		cout << (char)target;
	} else if (instr.instr == Iinc) {
		char c;
		cin >> c;
		inputReg = c;
	} else if (instr.instr == Iipc) {
		inputReg = cin.peek();
	} else if (instr.instr == Iinu) {
		cin >> inputReg; // NOTE inu maxes out on overflow
		cin.clear();
	} else if (instr.instr == Iinl) {
		char c = 0;
		while (c != '\n') cin >> c;
	}
	else unreachable();
}
void interpInstr(VM& vm, Instr& instr, bool& ipChanged) {
	unsigned short left, right;
	if (instr.hasImm()) right = instr.immediate;
	else if (instr.hasReg()) right = interpGetReg(vm, instr.suffixes.reg);

	if (instr.hasMod()) {
		left = interpGetReg(vm, InstrToModReg[instr.instr]);
		right = interpOperation(vm, instr.suffixes.modifier, left, right);
	}
	bool cond;
	if (instr.hasCond()) {
		cond = interpCond(vm, instr, (signed short) right);
	}
	interpInstrBody(vm, instr, right, cond, ipChanged);
}
void interpret(int startIdx) {
	globalVm.start(startIdx);
	cin.unsetf(ios_base::skipws); // set cin to not ignore whitespace
	while (globalVm.ip < parseCtx.instrs.size()) { // TODO infinite loop prevention
		bool ipChanged = false;
		interpInstr(globalVm, parseCtx.instrs[globalVm.ip], ipChanged);
		if (!ipChanged) globalVm.ip++;
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
		"extern ExitProcess\n"
		"extern GetStdHandle\n"
		"extern WriteFile\n"
		"extern ReadFile\n"
		"\n"
		"section .text\n"
		"exit: ; exits the program with code in rax\n"
		"	mov rcx, rax\n"
		"	and rsp, -16 ; force 16-byte alignment\n"
		"	sub rsp, 32\n"
		"	call ExitProcess\n"
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
		"	push QWORD 0 ; 128-bit align before call\n"
		"	call exit\n"
		"\n"
		"get_std_fds: ; prepares all std fds, regs unsafe!\n"
		"	sub rsp, 32 ; reserve shadow space\n"
		"	mov rcx, -10 ; stdin fd\n"
		"	call GetStdHandle\n"
		"	mov rcx, stdin_fd\n"
		"	mov [rcx], rax\n"
		"	mov rcx, -11 ; stdout fd\n"
		"	call GetStdHandle\n"
		"	mov rcx, stdout_fd\n"
		"	mov [rcx], rax\n"
		"	mov rcx, -12 ; stderr fd\n"
		"	call GetStdHandle\n"
		"	mov rcx, stderr_fd\n"
		"	mov [rcx], rax\n"
		"	add rsp, 32 ; remove shadow space\n"
		"	ret\n"
		"\n"
		"; rdx - buff, r8 - number of bytes -> rax - number written\n"
		"stdout_write:\n"
		"	mov rcx, QWORD [rel stdout_fd]\n"
		"	jmp write_file\n"
		"stderr_write:\n"
		"	mov rcx, QWORD [rel stderr_fd]\n"
		"	; fall through to write_file\n"
		"\n"
		"; rcx - fd, rdx - buff, r8 - chars / buffsize -> rax - number read/written\n"
		"write_file:\n"
		"	mov rax, WriteFile\n"
		"	jmp call_winapi_file_op\n"
		"read_file:\n"
		"	mov rax, ReadFile\n"
		"call_winapi_file_op:\n"
		"	mov rbp, rsp ; save rsp @ retval\n"
		"	and rsp, -16 ; force 16-byte alignment\n"
		"	push 0 ; number of bytes written/read var\n"
		"	mov r9, rsp ; ptr to that var\n"
		"	push 0 ; OVERLAPPED struct null ptr (5th arg) & still aligned\n"
		"	sub rsp, 32 ; reserve shadow space\n"
		"	call rax\n"
		"	add rsp, 32+8 ; remove shadow space & overlapped\n"
		"	pop rax ; num written / read\n"
		"	mov rsp, rbp\n"
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
		"	add rax, rcx ; rax = 10 * rax + rcx\n"
		"\n"
		"	mov r10, 65535\n"
		"	cmp rax, r10\n"
		"	cmova rax, r10\n"
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
	cout << "usage: Masfix [flags] <masfix-file-path>\n"
			"	flags:\n"
			"		-v / --verbose   - additional compilation messages\n"
			"		-S / --strict    - disables multiple errors\n"
			"		-i / --include   - additional include paths\n"
			"	mode:\n"
			"		-r / --run       - run executable after compilation\n"
			"		-I / --interpret - interpret instead of compile\n"
			"	side effects:\n"
			"		-A / --keep-asm  - keep assembly file\n"
			"		-D / --dump      - dumps prepocessed code into file\n";
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
fs::path _masfixFolder = fs::canonical("C:/Users/micha/source/Langs/Masfix"); // NOTE fs::canonical(__FILE__) DNW - mixes compile / runtime
fs::path checkPathArg(string arg, bool file) {
	fs::path p = fs::weakly_canonical(arg);
	if (!fs::exists(p)) p = _masfixFolder/p;
	checkUsage(fs::exists(p), "Invalid argument or file not found" errorQuoted(arg));
	if (file) checkUsage(fs::is_regular_file(p), "File expected" errorQuoted(arg));
	else if (fs::is_regular_file(p)) p = p.parent_path();
	return p;
}
/// include paths will be in this order: module-rel dir, input dir, --includes, CWD, std, Masfix dir
void populateIncludePaths(Flags& flags) {
	checkUsage(!flags.inputPath.empty(), "Input file path not found");
	flags.includeFolders.insert(flags.includeFolders.begin(), flags.inputPath.parent_path());
	flags.includeFolders.insert(flags.includeFolders.end(), fs::current_path());
	flags.includeFolders.insert(flags.includeFolders.end(), _masfixFolder/"std");
	flags.includeFolders.insert(flags.includeFolders.end(), _masfixFolder);
}
Flags processLineArgs(int argc, char *argv[]) {
	checkUsage(argc >= 2, "Insufficent number of command line args");
	Flags flags = Flags();
	for (int i = 1; i < argc; ++i) {
		string arg = argv[i];
		if (arg == "-v" || arg == "--verbose") {
			flags.verbose = true;
		} else if (arg == "-r" || arg == "--run") {
			flags.run = true;
		} else if (arg == "-I" || arg == "--interpret") {
			flags.interpret = true;
		} else if (arg == "-i" || arg == "--include") {
			checkUsage(++i < argc, "Include path expected");
			flags.includeFolders.push_back(checkPathArg(argv[i], false));
		} else if (arg == "-A" || arg == "--keep-asm") {
			flags.keepAsm = true;
		} else if (arg == "-S" || arg == "--strict") {
			FLAG_strictErrors = true;
		} else if (arg == "-D" || arg == "-d" || arg == "--dump") {
			flags.dump = true;
		} else {
			checkUsage(arg.at(0) != '-', "Unknown argument" errorQuoted(arg));
			flags.inputPath = checkPathArg(arg, true);
		}
	}
	populateIncludePaths(flags);
	return flags;
}
ifstream openInputFile(fs::path path) {
	ifstream ifs(path);
	checkCond(ifs.good(), "The input file" errorQuoted(path.string()) " couldn't be opened");
	return ifs;
}
ofstream openOutputFile(fs::path path) {
	ofstream ofs(path);
	checkCond(ofs.good(), "The output file" errorQuoted(path.string()) " couldn't be opened");
	return ofs;
}
int runCmdEchoed(vector<string> args, Flags& flags, bool exitOnErr=true) {
	string s = accumulate(args.begin() + 1, args.end(), args.front(), // concat args with spaces
		[](string s0, const string& s1) { return s0 += " " + s1; });
	const char* command = s.c_str();
	if (flags.verbose) {
		cout << "[CMD] " << command << '\n';
	}
	int returnCode = system(command);
	if (exitOnErr && returnCode) {
		exit(returnCode);
	}
	return returnCode;
}
void removeFile(fs::path file) {
	bool ret = remove(file.string().c_str());
	if (ret) {
		cerr << "WARNING: error on removing the file '" << file.filename() << "'\n";
	}
}
int compileAndRun(Flags& flags) {
	runCmdEchoed({
		"nasm", "-fwin64", "-g", "-F cv8",
		flags.filePathStr("asm")
	}, flags);
	runCmdEchoed({
		"gcc", "-nostartfiles", "-Wl,-e,_start", "-lkernel32",
		"-o", flags.filePathStr("exe"), "-g", flags.filePathStr("obj")
	}, flags);
	if (flags.keepAsm) {
		cout << "[NOTE] asm file: " << flags.filePath("asm") << ":180:1\n";
	} else {
		removeFile(flags.filePath("asm"));
	}
	removeFile(flags.filePath("obj"));
	if (flags.run) return runCmdEchoed({flags.filePathStr("exe")}, flags, false);
	return 0;
}
void initParseCtx(Flags& flags, string mainRelPath) {
	if (flags.dump) parseCtx.dumpFile = openOutputFile(flags.filePath("dump"));
	parseCtx.strToLabel = {{"begin", Label("begin", 0, Loc(mainRelPath, 1, 1))}, {"end", Label("end", 0, Loc(mainRelPath, 1, 1))}};
}
void run(Flags& flags) {
	int exitCode = 0;
	if (flags.interpret) {
		interpret();
	} else {
		ofstream outFile = openOutputFile(flags.filePath("asm"));
		generate(outFile, parseCtx.instrs);

		exitCode = compileAndRun(flags);
	}
	exit(exitCode);
}
int main(int argc, char *argv[]) {
	flags = processLineArgs(argc, argv);
	Scope scope;
	string mainRelPath = tokenizeNewModule(flags.inputPath, scope, true);
	initParseCtx(flags, mainRelPath);

	preprocess(scope);

	parseCtx.close();
	if (flags.dump) cout << "\n[NOTE] dump file: \"" << flags.filePath("dump").string() << "\"\n";
	raiseErrors();

	run(flags);
}
