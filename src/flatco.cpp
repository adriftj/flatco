#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <string>
#include <string_view>
#include <map>
#include <set>
#include "getopt.h"

const char* const k_progname = "flatco";
const char* const k_version = "0.1";
const char* const k_helpstr =
"Usage:\n"
"flatco <options> <input_filename>\n"
"Options:\n"
"  -o,  --output <output_filename> Specify output file name\n"
"  -v,  --version                  Display version\n"
"  -h,  --help                     Display this help\n"
;

static const char* short_opts = "o:vh";

namespace LongOpts {
    enum {
        version = 'v',
        help = 'h',
        output = 'o',
    };
}

static const struct option long_opts[] = {
    { "version", no_argument,       NULL, LongOpts::version },
    { "help",    no_argument,       NULL, LongOpts::help    },

    { "output",  required_argument, NULL, LongOpts::output  },

    { NULL,           no_argument,  NULL,  0                }
};

static const char* s_outFileName = nullptr;
static const char* s_inFileName = nullptr;

int processing_cmd(int argc, char* const argv[]) {
    int opt;

    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
        case LongOpts::help:
            puts(k_helpstr);
            return 1;

        case LongOpts::version:
            printf("%s: version %s\n", k_progname, k_version);
            return 1;

        case LongOpts::output:
            s_outFileName = optarg;
            break;

        default:
            puts("for more detail see help\n");
            break;
        }
    }
    if (optind >= argc)
        return 1;
    s_inFileName = argv[optind++];
    return 0;
}

enum ItemKind { CODE=0, BL_func, BL_call, BL_return };

bool IsIdentFirst(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool IsIdentOther(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

bool IsSpaceChar(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

ItemKind CheckKeyword(const char* p, size_t n) {
    if (n < 7 || p[0]!='B' || p[1]!='L' || p[2]!='_')
        return CODE;
    if (!strncmp(p+3, "func", n-3))
        return BL_func;
    if (!strncmp(p+3, "call", n-3))
        return BL_call;
    if (!strncmp(p+3, "return", n-3))
        return BL_return;
    return CODE;
}

struct Lexer;

struct BlError {
    size_t row;
    size_t col;
    std::string s;

    BlError(size_t rowA, size_t colA, const char* sA) : row(rowA), col(colA), s(sA) {}
    BlError(const Lexer& lex, const char* sA);
};

struct Token {
    size_t row;
    size_t col;
    const char* s;
    size_t len;
};

class Lexer {
    const char* src_;
    const char* pe_;

    const char* p_;
    size_t row_;
    size_t col_;

public:
    Lexer(const char* s, size_t len) : src_(s), p_(s-1), pe_(s + len), row_(0), col_(1) {}
    Lexer(const Lexer& lex, const char* s, size_t len, size_t row, size_t col) : src_(lex.src_), p_(s-1), pe_(s + len), row_(row), col_(col) {}

    void reset(const char* s, size_t len, size_t row, size_t col) {
        p_ = s - 1;
        pe_ = s + len;
        row_ = row;
        col_ = col;
    }

    const char* curP() const { return p_; }
    size_t curRow() const { return row_; }
    size_t curCol() const { return col_; }

    char get() {
        const char* p = p_;
        if (p + 1 >= pe_) {
            p_ = pe_;
            return 0;
        }

        char c = *++p_;
        if (p < src_ || *p == '\n') {
            ++row_;
            col_ = 0;
        }
        if (c != '\r')
            ++col_;
        return c;
    }

    char skipGet(size_t n) {
        p_ += n;
        col_ += n;
        assert(p_ <= pe_);
        if (p_ < pe_)
            return *p_;
        return 0;
    }

    char skipSkipBlanksGet(size_t n) {
        return skipBlanks(skipGet(n));
    }

    char backward(size_t n) {
        p_ -= n;
        col_ -= n;
        assert(p_ >= src_);
        return *p_;
    }

    void savePos(size_t& row, size_t& col, const char*& p) {
        row = row_;
        col = col_;
        p = p_;
    }

    void loadPos(size_t row, size_t col, const char* p) {
        row_ = row;
        col_ = col;
        p_ = p;
    }

    void skipMultilineComment() {
        bool findStar = false;
        char c = get();
        while (c) {
            if (!findStar) {
                if (c == '*')
                    findStar = true;
            }
            else if (c == '/')
                return;
            else
                findStar = false;
            c = get();
        }
        throw BlError(row_, col_, "Multi-line comments are not closed");
    }

    char skipCommentsGet() {
        char c = get();
        if (c != '/')
            return c;

        size_t row, col;
        const char* p;
        savePos(row, col, p);
        c = get();
        if (c == '*')
            skipMultilineComment();
        else if (c == '/') {
            c = get();
            while (c && c != '\n')
                c = get();
        }
        else {
            loadPos(row, col, p);
            return '/';
        }
        return ' ';
    }

    Token getString(char start) {
        const char* p;
        size_t row, col;
        savePos(row, col, p);
        char c = get();
        while (c && c != start) {
            if (c == '\n')
                throw BlError(row_, col_, "String cross over line");
            if (c == '\\') {
                c = get();
                if (c)
                    c = get();
            }
            else
                c = get();
        }
        if (!c)
            throw BlError(row_, col_, "String hasn't end");
        return Token{ .row = row, .col=col, .s = p, .len = 1+(size_t)(p_ - p) };
    }

    Token peekIdent() {
        const char* p;
        size_t row, col;
        savePos(row, col, p);
        assert(IsIdentFirst(*p));
        const char* q = p+1;
        while (q < pe_) {
            char c = *q;
            if (!IsIdentOther(c))
                break;
            ++q;
        }
        size_t len = q - p;
        return Token{ .row = row, .col = col, .s = p, .len = len };
    }

    Token getIdent() {
        Token tok = peekIdent();
        size_t n = tok.len - 1;
        p_ += n;
        col_ += n;
        return tok;
    }

    size_t getSizeFrom(const char* last) {
        assert(p_ > last);
        return p_ - last;
    }

    char skipBlanks(char c) {
        while (IsSpaceChar(c))
            c = skipCommentsGet();
        return c;
    }

    char skipBlanksGet() {
        return skipBlanks(skipCommentsGet());
    }

    Token getBrackets(char start) {
        const char* p;
        size_t row, col;
        savePos(row, col, p);
        char end;
        if (start == '(')
            end = ')';
        else if (start == '[')
            end = ']';
        else if (start == '{')
            end = '}';
        else if (start == '<')
            end = '>';
        else
            throw BlError(row_, col_, "Not a left bracket");
        int level = 1;
        char c = skipCommentsGet();
        while (c) {
            if (c == end) {
                if (level <= 0)
                    throw BlError(row_, col_, "No matched left bracket");
                --level;
                if (level == 0)
                    return Token{ .row = row, .col = col, .s = p, .len = 1+(size_t)(p_ - p) };
            }
            else if (c == start)
                ++level;
            else if (c == '"' || c == '\'')
                getString(c);
            c = skipCommentsGet();
        }
        throw BlError(row_, col_, "No matched right bracket till end of file");
    }

    Token getIdentSkipBlanks(char c) {
        if (!IsIdentFirst(c))
            throw BlError(row_, col_, "Identifier should start with A-Za-z_");
        return getIdent();
    }

    bool getType(Token& tok, char& ch) {
        char c = skipBlanksGet();
        const char* p;
        size_t row, col;
        savePos(row, col, p);
        bool gotTypeName = false;
        for (;;) {
            if (gotTypeName) {
                if (IsIdentFirst(c)) {
                    Token tokN = getIdent();
                    if (strncmp(tokN.s, "const", tokN.len) != 0 && strncmp(tokN.s, "volatile", tokN.len) != 0) {
                        ch = backward(tokN.len - 1);
                        tok = Token{ .row = row, .col = col, .s = p, .len = getSizeFrom(p) };
                        return true;
                    }
                }
                else if (c == '<')
                    getBrackets(c);
                else if(c != '*' && c != '&') {
                    tok = Token{ .row = row, .col = col, .s=p, .len=getSizeFrom(p) };
                    ch = c;
                    return true;
                }
            }
            else {
                if (!IsIdentFirst(c)) {
                    ch = c;
                    return false;
                }
                Token tokN = getIdent();
                gotTypeName = (strncmp(tokN.s, "const", tokN.len)!=0 && strncmp(tokN.s, "volatile", tokN.len)!=0);
            }
            c = skipBlanksGet();
        }
    }

    Token getExpr(char& c, char end) {
        c = skipBlanksGet();
        const char* p;
        size_t row, col;
        savePos(row, col, p);
        while(c && c!=end) {
            if (c == '"' || c == '\'')
                getString(c);
            else if (c == '{' || c == '[' || c == '(')
                getBrackets(c);
            c = skipBlanksGet();
        }
        size_t len = p_ - p;
        return Token{ .row = row, .col = col, .s = p, .len = len};
    }
};

BlError::BlError(const Lexer& lex, const char* sA) : row(lex.curRow()), col(lex.curCol()), s(sA) {}

struct SeqInsertable {
    std::string_view s;
    std::vector<size_t> seqPositions;
};

struct CxxItem {
    size_t row;
    size_t col;
    ItemKind kind;
    SeqInsertable s;
    size_t index; // of Parser::funcs_, Parser::calls_, FuncItem::calls, FuncItem::returns
};

struct FuncParam {
    Token type;
    Token name;
};

struct ReturnItem {
    size_t row;
    size_t col;
    SeqInsertable seqInsertable;
};

struct CallItem {
    size_t row;
    size_t col;
    std::string_view name; // BL_func name
    SeqInsertable lval;
    std::vector<SeqInsertable> params;
    size_t funcIndex;
};

struct FuncItem {
    Token name;
    std::vector<FuncParam> params;
    std::map<std::string, size_t> paramIndexes;
    std::vector<CxxItem> items;
    std::vector<ReturnItem> returns;
    std::vector<CallItem> calls;
    std::vector<size_t> callers;
    bool retvoid;
};

bool CheckParamPrefix(const char* s, const char* src) {
    while (--s >= src) {
        char c = *s;
        if (IsSpaceChar(c))
            continue;
        if (c == '.')
            return false;
        if (--s < src)
            return true;
        char c2 = *s;
        if (c2 == ':' && c == ':' || c2 == '-' && c == '>' || c2 == '.' && c == '*')
            return false;
        if (--s < src)
            return true;
        char c3 = *s;
        return !(c3 == '-' && c2 == '>' && c == '*');
    }
    return true;
}

SeqInsertable FindParams(const std::string_view& s, const std::map<std::string, size_t>& paramIndexes) {
    std::vector<size_t> positions;
    if (paramIndexes.size() > 0) {
        Lexer lex(s.data(), s.size());
        char c = lex.skipBlanksGet();
        while (c) {
            if (IsIdentFirst(c)) {
                Token tok = lex.getIdent();
                auto it = paramIndexes.find(std::string(tok.s, tok.len));
                if (it != paramIndexes.cend()) {
                    if (CheckParamPrefix(tok.s, s.data()))
                        positions.push_back(tok.s - s.data());
                }
            }
            c = lex.skipBlanksGet();
        }
    }
    return SeqInsertable{ .s = s, .seqPositions = positions };
}

void ParseBlCall(Lexer& lex, std::vector<CxxItem>& items, std::vector<CallItem>& calls, const std::map<std::string, size_t>& paramIndexes) {
    char c = lex.skipSkipBlanksGet(7); // strlen("BL_call")
    if (c != '(')
        throw BlError(lex, "Should be '(' after BL_call");
    Token tok = lex.getBrackets(c);

    Lexer callLex(lex, tok.s + 1, tok.len - 2, tok.row, tok.col+1);
    Token tokLval = callLex.getExpr(c, '=');
    SeqInsertable lval;
    if (c == '=') {
        if (tokLval.len <= 0)
            throw BlError(callLex, "BL_call expected left value before '='");
        lval = FindParams(std::string_view(tokLval.s, tokLval.len), paramIndexes);
        const char* p;
        size_t row, col;
        callLex.savePos(row, col, p);
        callLex.reset(p + 1, tok.len - 3 - (p - tokLval.s), row, col + 1);
    }
    else
        callLex.reset(tok.s + 1, tok.len - 2, tok.row, tok.col + 1);

    Token tokName = callLex.getIdentSkipBlanks(callLex.skipBlanksGet());
    c = callLex.skipBlanksGet();
    if (c != '(')
        throw BlError(callLex, "Should be '(' after function name");

    Token tokParams = callLex.getBrackets(c);
    c = callLex.skipBlanksGet();
    if (c)
        throw BlError(callLex, "BL_call syntax error after ')'");

    Lexer paramLex(callLex, tokParams.s + 1, tokParams.len - 2, tokParams.row, tokParams.col+1);
    std::vector<SeqInsertable> params;
    Token tokPara = paramLex.getExpr(c, ',');
    for (;;) {
        if (tokPara.len > 0)
            params.push_back(FindParams(std::string_view(tokPara.s, tokPara.len), paramIndexes));
        if (c != ',')
            break;
        tokPara = paramLex.getExpr(c, ',');
    }
    if (c)
        throw BlError(paramLex, "',' expected");

    calls.emplace_back(tokName.row, tokName.col, std::string_view(tokName.s, tokName.len), lval, params, 0);
    items.emplace_back(tok.row, tok.col, BL_call, SeqInsertable{}, calls.size() - 1);
}

void ParseBlReturn(Lexer& lex, std::vector<CxxItem>& items, std::vector<ReturnItem>& returns, const std::map<std::string, size_t>& paramIndexes) {
    char c = lex.skipSkipBlanksGet(9); // strlen("BL_return")
    if (c != '(')
        throw BlError(lex, "Should be '(' after BL_return");
    Token tok = lex.getBrackets(c);
    returns.emplace_back(tok.row, tok.col, FindParams(std::string_view(tok.s+1, tok.len-2), paramIndexes));
    items.emplace_back(tok.row, tok.col, BL_return, SeqInsertable{}, returns.size()-1);
}

class Parser {
    Lexer lex_;
    std::vector<CxxItem> items_;
    std::vector<FuncItem> funcs_;
    std::vector<CallItem> calls_;
    std::map<std::string, size_t> name2Func_;

    void checkAddCode(size_t row, size_t col, const char* p) {
        size_t n = lex_.getSizeFrom(p);
        if (n > 0)
            items_.emplace_back(row, col, CODE, SeqInsertable{ std::string_view(p, n), {} }, 0);
    }

    void parseBlFunc();
    void prepare();
    void expand(FILE* fOut, const char* srcFileName, size_t funcIndex, const std::string& lval, const std::vector<std::string>& params, size_t& seq);

public:
    Parser(const char* src, size_t len);

    void gen(FILE* fOut, const char* srcFileName);
};

void Parser::parseBlFunc() {
    const char* p0;
    size_t row0, col0;
    lex_.savePos(row0, col0, p0);

    char c = lex_.skipSkipBlanksGet(7); // strlen("BL_func")
    if (c != '(')
        throw BlError(lex_, "Shoud be '(' following BL_func");
    lex_.getBrackets(c);

    Token tokRetType;
    if(!lex_.getType(tokRetType, c))
        throw BlError(lex_, "BL_func return type expected");
    Token tokFuncName = lex_.getIdentSkipBlanks(c);
    c = lex_.skipBlanksGet();

    if (c != '(')
        throw BlError(lex_, "Should be '(' after function name");
    Token tokParams = lex_.getBrackets(c);
    Lexer paramLex(lex_, tokParams.s+1, tokParams.len-2, tokParams.row, tokParams.col);
    Token tokParamType;
    std::vector<FuncParam> params;
    std::map<std::string, size_t> paramIndexes;
    while (paramLex.getType(tokParamType, c)) {
        Token tokParamName = paramLex.getIdentSkipBlanks(c);
        params.emplace_back(tokParamType, tokParamName);
        size_t idx = params.size() - 1;
        if (!paramIndexes.insert(std::make_pair(std::string(tokParamName.s, tokParamName.len), idx)).second)
            throw BlError(tokParamName.row, tokParamName.col, "BL_func parameter is duplicated");
        c = paramLex.skipBlanksGet();
        if (c != ',')
            break;
    }
    if(c)
        throw BlError(paramLex, "Syntax error or missing ','");

    c = lex_.skipBlanksGet();
    if(c != '{')
        throw BlError(lex_, "Should be '{' after function prototype");
    Token tokBody = lex_.getBrackets(c);
    Lexer bodyLex(lex_, tokBody.s+1, tokBody.len-2, tokBody.row, tokBody.col+1);

    size_t row, col;
    const char* p;
    c = bodyLex.skipCommentsGet();
    bodyLex.savePos(row, col, p);
    std::vector<CxxItem> items;
    std::vector<ReturnItem> returns;
    std::vector<CallItem> calls;
    while (c) {
        if (c == '"' || c == '\'') {
            bodyLex.getString(c);
            c = bodyLex.skipCommentsGet();
        }
        else if (IsIdentFirst(c)) {
            Token tok = bodyLex.peekIdent();
            ItemKind kind = CheckKeyword(tok.s, tok.len);
            if (kind != CODE) {
                size_t n = bodyLex.getSizeFrom(p);
                if (n > 0) {
                    SeqInsertable s = FindParams(std::string_view(p, n), paramIndexes);
                    items.emplace_back(row, col, CODE, s, 0);
                }
            }
            if (kind == BL_return || kind == BL_call) {
                if (kind == BL_return)
                    ParseBlReturn(bodyLex, items, returns, paramIndexes);
                else
                    ParseBlCall(bodyLex, items, calls, paramIndexes);
                c = bodyLex.skipCommentsGet();
                bodyLex.savePos(row, col, p);
            }
            else if (kind == BL_func)
                throw BlError(tok.row, tok.col, "Can't use BL_func inside BL_func");
            else
                c = bodyLex.skipSkipBlanksGet(tok.len);
        }
        else
            c = bodyLex.skipCommentsGet();
    }
    size_t n = bodyLex.getSizeFrom(p);
    if (n > 0) {
        SeqInsertable s = FindParams(std::string_view(p, n), paramIndexes);
        items.emplace_back(row, col, CODE, s, 0);
    }

    funcs_.emplace_back(tokFuncName, params, paramIndexes, items, returns, calls, std::vector<size_t>{}, true);
    items_.emplace_back(row0, col0, BL_func, SeqInsertable{}, funcs_.size()-1);
}

void Parser::prepare() {
    size_t nFuncs = funcs_.size();
    for (size_t i = 0; i < nFuncs; ++i) {
        auto& func = funcs_[i];
        std::string funcName(func.name.s, func.name.len);
        if (!name2Func_.insert(std::make_pair(funcName, i)).second)
            throw BlError(func.name.row, func.name.col, "Duplicated BL_func");
        bool first = true;
        for (auto& ret : func.returns) {
            bool retvoid = ret.seqInsertable.s.empty();
            if (first) {
                first = false;
                func.retvoid = retvoid;
            }
            else if (func.retvoid != retvoid)
                throw BlError(ret.row, ret.col, "Multiple BL_return returns are inconsistent, some have no return value, some have");
        }
    }

    std::map<size_t, std::set<size_t>> callDag;
    for (size_t i = 0; i < nFuncs; ++i)
        callDag[i] = std::set<size_t>{};
    for (size_t i = 0; i < nFuncs; ++i) {
        auto& func = funcs_[i];
        for (auto& callItem : func.calls) {
            std::string callee(callItem.name);
            auto it = name2Func_.find(callee);
            if (it == name2Func_.cend()) {
                throw BlError(callItem.row, callItem.col, "BL_call undefined BL_func");
            }
            if (it->second == i)
                throw BlError(callItem.row, callItem.col, "BL_call itself");
            callItem.funcIndex = it->second;
            funcs_[it->second].callers.push_back(i);

            if (callItem.params.size() != funcs_[it->second].params.size())
                throw BlError(callItem.row, callItem.col, "The number of parameters of the calling and called functions are not equal");
            if (!callItem.lval.s.empty() && funcs_[it->second].retvoid)
                throw BlError(callItem.row, callItem.col, "The caller needs a return value but the called BL_func returns void");

            auto it2 = callDag.find(i);
            assert(it2 != callDag.cend());
            it2->second.insert(it->second);
        }
    }

    for (auto& callItem : calls_) {
        std::string callee(callItem.name);
        auto it = name2Func_.find(callee);
        if (it == name2Func_.cend()) {
            throw BlError(callItem.row, callItem.col, "BL_call undefined BL_func");
        }
        callItem.funcIndex = it->second;
    }

    std::vector<size_t> sorted;
    bool foundLeaf;
    do {
        foundLeaf = false;
        for (auto it = callDag.begin(); it != callDag.end(); ) {
            size_t funcIndex = it->first;
            foundLeaf = (it->second.size() == 0);
            auto itNext = it;
            ++itNext;
            if (foundLeaf) {
                sorted.push_back(funcIndex);
                callDag.erase(it);
                for (auto caller : funcs_[funcIndex].callers)
                    callDag[caller].erase(funcIndex);
            }
            it = itNext;
        }
    } while (foundLeaf);
    if (sorted.size() != funcs_.size()) {
        assert(callDag.size() > 0);
        std::string funcNames;
        size_t row = 0, col = 0;
        for (auto i : callDag) {
            auto& func = funcs_[i.first];
            funcNames += " " + std::string(func.name.s, func.name.len);
            if (row == 0) {
                row = func.name.row;
                col = func.name.col;
            }
        }
        throw BlError(row, col, std::string("There is recursive calls:" + funcNames).c_str());
    }
}

Parser::Parser(const char* src, size_t len) : lex_(src, len) {
    std::map<std::string, size_t> emptyParamIndexes;
    size_t row, col;
    const char* p;
    char c = lex_.skipCommentsGet();
    lex_.savePos(row, col, p);
    while (c) {
        if (c == '"' || c == '\'') {
            lex_.getString(c);
            c = lex_.skipCommentsGet();
        }
        else if (IsIdentFirst(c)) {
            Token tok = lex_.peekIdent();
            ItemKind kind = CheckKeyword(tok.s, tok.len);
            if (kind != CODE) {
                checkAddCode(row, col, p);
                if (kind == BL_func || kind == BL_call) {
                    if (kind == BL_func)
                        parseBlFunc();
                    else
                        ParseBlCall(lex_, items_, calls_, emptyParamIndexes);
                    c = lex_.skipCommentsGet();
                    lex_.savePos(row, col, p);
                }
                else if (kind == BL_return)
                    throw BlError(tok.row, tok.col, "Can't use BL_return outside BL_func");
                else
                    assert(false);
            }
            else
                c = lex_.skipSkipBlanksGet(tok.len);
        }
        else
            c = lex_.skipCommentsGet();
    }
    checkAddCode(row, col, p);
    prepare();
}

std::string FromSeqInsertable(const SeqInsertable& si, size_t seq) {
    size_t n = si.seqPositions.size();
    if (n <= 0)
        return std::string(si.s);
    char buf[32];
    snprintf(buf, sizeof(buf), "_BLparam%zx_" , seq);
    std::string s(si.s.substr(0, si.seqPositions[0]));
    for (size_t i = 1; i < n; ++i) {
        s += buf;
        s += si.s.substr(si.seqPositions[i - 1], si.seqPositions[i] - si.seqPositions[i - 1]);
    }
    s += buf;
    s += si.s.substr(si.seqPositions[n-1]);
    return s;
}

bool CheckBlInclude(const std::string_view& s) {
    Lexer lex(s.data(), s.size());
    char c = lex.skipBlanksGet();
    while (c) {
        if (c != '#')
            return false;
        c = lex.skipBlanksGet();
        if (!IsIdentFirst(c))
            return false;
        Token tok = lex.getIdent();
        if (strncmp(tok.s, "include", tok.len) != 0)
            return false;
        c = lex.skipBlanksGet();
        if (c != '<' && c != '"')
            return false;
        if (c == '<')
            c = '>';
        const char* p0 = lex.curP();
        const char* p = s.data() + s.size();
        while (--p >= p0) {
            char c2 = *p;
            if (IsSpaceChar(c2))
                continue;
            if (c2 != c)
                return false;
            break;
        }
        if (p0 + 8 >= p) // 8==strlen("flatco.h")
            return false;
        if (strncmp(p - 8, "flatco.h", 8) != 0)
            return false;
        if (p0 + 9 == p)
            return true;
        return p[-9] == '/';
    }
    return false;
}

void GetRidBlInclude(FILE* fOut, const std::string_view& s) {
    size_t off = 0, pos;
    for (size_t off=0;; off = pos+1) {
        pos = s.find_first_of('\n', off);
        std::string_view t = (pos == s.npos ? s.substr(off) : s.substr(off, pos - off));
        if (CheckBlInclude(t)) {
            if(off > 0)
                fwrite(s.data(), 1, off, fOut);
            fputs("//", fOut);
            fwrite(s.data()+off, 1, s.size()-off, fOut);
            return;
        }
        if (pos == s.npos)
            break;
    }
    fwrite(s.data(), 1, s.size(), fOut);
}

void Parser::gen(FILE* fOut, const char* srcFileName) {
    size_t seq = 0;
    bool firstCode = true;
    for (auto& item : items_) {
        if (item.kind == CODE) {
            assert(item.s.s.size() > 0);
            fprintf(fOut, "\n#line %zu \"%s\"\n", item.row, srcFileName);
            if (firstCode) {
                firstCode = false;
                GetRidBlInclude(fOut, item.s.s);
            }
            else
                fwrite(item.s.s.data(), 1, item.s.s.size(), fOut);
        }
        else if (item.kind == BL_call) {
            CallItem call = calls_[item.index];
            std::vector<std::string> params;
            for (auto& v : call.params)
                params.push_back(std::string(v.s));
            expand(fOut, srcFileName, call.funcIndex, std::string(call.lval.s), params, seq);
        }
        else {
            assert(item.kind == BL_func);
        }
    }
}

void Parser::expand(FILE* fOut, const char* srcFileName, size_t funcIndex, const std::string& lval, const std::vector<std::string>& params, size_t& seq) {
    const FuncItem& func = funcs_[funcIndex];
    size_t seqCurrent = seq++;
    fputs("do {", fOut);
    assert(params.size() == func.params.size());
    size_t i = 0;
    for (auto& pi : func.params) {
        fprintf(fOut, "%s _BLparam%zx_%s=%s;", std::string(pi.type.s, pi.type.len).c_str(), seqCurrent, std::string(pi.name.s, pi.name.len).c_str(), params[i].c_str());
        ++i;
    }
    for (auto& item : func.items) {
        if (item.kind == CODE) {
            assert(item.s.s.size() > 0);
            std::string s = FromSeqInsertable(item.s, seqCurrent);
            fprintf(fOut, "\n#line %zu \"%s\"\n%s", item.row, srcFileName, s.c_str());
        }
        else if (item.kind == BL_call) {
            CallItem call = func.calls[item.index];
            std::vector<std::string> params;
            for (auto& v : call.params)
                params.push_back(FromSeqInsertable(v, seqCurrent));
            expand(fOut, srcFileName, call.funcIndex, FromSeqInsertable(call.lval, seqCurrent), params, seq);
        }
        else if (item.kind == BL_return) {
            bool lvalEmpty = lval.empty();
            ReturnItem ri = func.returns[item.index];
            std::string rets = FromSeqInsertable(ri.seqInsertable, seqCurrent);
            fprintf(fOut, "do{ %s%c%s; goto _BLexit%zx; }while(0)", lvalEmpty? "": lval.c_str(), lvalEmpty? ' ': '=', rets.c_str(), seqCurrent);
        }
        else {
            assert(false);
        }
    }
    fprintf(fOut, "_BLexit%zx:;}while(0)", seqCurrent);
}

int main(int argc,char* const* argv) {
    if (processing_cmd(argc, argv))
        return 1;
    FILE* fIn = fopen(s_inFileName, "r");
    if (!fIn) {
        printf("Can't open file '%s'.\n", s_inFileName);
        return 1;
    }
    fseek(fIn, 0, SEEK_END);
    size_t bufLen = ftell(fIn);
    char* src = (char*)malloc(bufLen);
    int r = 1;
    if (src) {
        fseek(fIn, 0, SEEK_SET);
        size_t len = fread(src, 1, bufLen, fIn);
        if (ferror(fIn)==0) {
            try {
                Parser parser(src, len);
                FILE* fOut = fopen(s_outFileName, "w");
                if (fOut) {
                    parser.gen(fOut, s_inFileName);
                    r = 0;
                    fclose(fOut);
                }
            }
            catch (BlError& err) {
                printf("At %zu:%zu: %s\n", err.row, err.col, err.s.c_str());
            }
        }
        else
            printf("Read input file error.\n");
        free(src);
    }
    fclose(fIn);
	return r;
}
