#include "parser.h"
#include <set>
#include <sstream>


static const std::set<std::string> BIN_OPS = {
    "+", "-", "*", "/", "%",
    "==", "!=", "<", ">", "<=", ">=",
    "&&", "||"
};

static NodePtr makeNode(NodeKind k) {
    auto n = std::make_shared<Node>();
    n->kind = k;
    return n;
}


struct Parser {
    const std::vector<Token>& toks;   // входной поток токенов
    size_t pos = 0;                   // индекс текущего токена
    std::vector<std::string> errors;  // накопленные сообщения об ошибках

    explicit Parser(const std::vector<Token>& t) : toks(t) {}

    bool atEnd() const { return pos >= toks.size(); }

    const Token& peek(size_t k = 0) const {
        static const Token eof{ TokType::DELIMITER, "<EOF>", 0, 0 };
        if (pos + k >= toks.size()) return eof;
        return toks[pos + k];
    }

    Token consume() {
        if (atEnd()) return peek();
        return toks[pos++];
    }

    bool checkType(TokType t) const {
        return !atEnd() && peek().type == t;
    }
    bool checkExact(TokType t, const std::string& v) const {
        return checkType(t) && peek().value == v;
    }

    bool matchExact(TokType t, const std::string& v) {
        if (checkExact(t, v)) { pos++; return true; }
        return false;
    }

    //запись ошибок
    std::string here() const {
        if (atEnd()) return "конец потока токенов";
        return "строка " + std::to_string(peek().line) +
            ", столбец " + std::to_string(peek().col);
    }

    void error(const std::string& msg) {
        errors.push_back("Ошибка [" + here() + "]: " + msg);
    }

    bool expect(TokType t, const std::string& v, const std::string& what) {
        if (checkExact(t, v)) { pos++; return true; }
        error("ожидался " + what + " '" + v + "', получено '" + peek().value + "'");
        return false;
    }

    bool expectType(TokType t, const std::string& what) {
        if (checkType(t)) return true;
        error("ожидался " + what + ", получено '" + peek().value + "'");
        return false;
    }

    void syncToStmtBoundary() {
        while (!atEnd()) {
            if (checkExact(TokType::DELIMITER, ";")) { pos++; return; }
            if (checkExact(TokType::DELIMITER, "}")) return;
            pos++;
        }
    }

    NodePtr parseProgram() {
        auto prog = makeNode(NodeKind::Program);
        while (!atEnd()) {
            if (checkExact(TokType::KEYWORD, "fn")) {
                auto fn = parseFunctionDecl();
                if (fn) prog->items.push_back(fn);
            }
            else {
                error("ожидалось объявление функции (ключевое слово 'fn')");
                pos++;
            }
        }
        return prog;
    }

    NodePtr parseFunctionDecl() {
        expect(TokType::KEYWORD, "fn", "ключевое слово");

        auto fn = makeNode(NodeKind::FunctionDecl);

        if (checkType(TokType::IDENTIFIER)) {
            fn->name = consume().value;
        }
        else {
            error("ожидалось имя функции (IDENTIFIER)");
            fn->name = "<error>";
        }

        expect(TokType::DELIMITER, "(", "разделитель");
        if (!checkExact(TokType::DELIMITER, ")")) {
            while (true) {
                auto p = parseParam();
                if (p) fn->params.push_back(p);
                if (matchExact(TokType::DELIMITER, ",")) continue;
                break;
            }
        }
        expect(TokType::DELIMITER, ")", "разделитель");

        if (matchExact(TokType::OPERATOR, "->")) {
            fn->hasReturnType = true;
            fn->typeName = parseType();
        }

        fn->body = parseBlock();
        return fn;
    }

    NodePtr parseParam() {
        auto p = makeNode(NodeKind::ParamDecl);
        if (checkType(TokType::IDENTIFIER)) {
            p->name = consume().value;
        }
        else {
            error("ожидалось имя параметра");
            p->name = "<error>";
        }
        expect(TokType::DELIMITER, ":", "разделитель");
        p->typeName = parseType();
        return p;
    }

    std::string parseType() {
        if (checkType(TokType::KEYWORD) || checkType(TokType::IDENTIFIER)) {
            return consume().value;
        }
        error("ожидалось имя типа");
        return "<error>";
    }

    NodePtr parseBlock() {
        auto blk = makeNode(NodeKind::Block);
        expect(TokType::DELIMITER, "{", "разделитель");

        while (!checkExact(TokType::DELIMITER, "}") && !atEnd()) {
            if (checkExact(TokType::KEYWORD, "let")) {
                auto v = parseVarDecl();
                if (v) blk->stmts.push_back(v);
            }
            else if (checkExact(TokType::KEYWORD, "if")) {
                auto i = parseIfStmt();
                if (i) blk->stmts.push_back(i);
            }
            else if (checkExact(TokType::KEYWORD, "while")) {
                auto w = parseWhileStmt();
                if (w) blk->stmts.push_back(w);
            }
            else if (checkExact(TokType::KEYWORD, "for")) {
                auto f = parseForStmt();
                if (f) blk->stmts.push_back(f);
            }
            else {
                bool looksLikeAssign =
                    checkType(TokType::IDENTIFIER) &&
                    peek(1).type == TokType::OPERATOR &&
                    peek(1).value == "=";

                if (looksLikeAssign) {
                    auto a = parseAssignStmt();
                    if (a) blk->stmts.push_back(a);
                }
                else {
                    auto e = parseExpression();
                    if (matchExact(TokType::DELIMITER, ";")) {
                        if (e) blk->stmts.push_back(e);
                    }
                    else if (checkExact(TokType::DELIMITER, "}")) {
                        blk->returnExpr = e;
                        break;
                    }
                    else {
                        error("ожидался ';' после выражения");
                        if (e) blk->stmts.push_back(e);
                        syncToStmtBoundary();
                    }
                }
            }
        }
        expect(TokType::DELIMITER, "}", "разделитель");
        return blk;
    }

    NodePtr parseVarDecl() {
        expect(TokType::KEYWORD, "let", "ключевое слово");
        auto v = makeNode(NodeKind::VarDecl);

        if (matchExact(TokType::KEYWORD, "mut")) v->isMutable = true;

        if (checkType(TokType::IDENTIFIER)) {
            v->name = consume().value;
        }
        else {
            error("ожидалось имя переменной");
            v->name = "<error>";
        }

        expect(TokType::DELIMITER, ":", "разделитель");
        v->typeName = parseType();
        expect(TokType::OPERATOR, "=", "оператор присваивания");
        v->init = parseExpression();
        expect(TokType::DELIMITER, ";", "разделитель");
        return v;
    }

    NodePtr parseAssignStmt() {
        auto a = makeNode(NodeKind::AssignStmt);

        auto lhs = makeNode(NodeKind::Identifier);
        lhs->name = consume().value;    
        a->left = lhs;

        expect(TokType::OPERATOR, "=", "оператор присваивания");
        a->right = parseExpression();
        expect(TokType::DELIMITER, ";", "разделитель");
        return a;
    }

    NodePtr parseIfStmt() {
        expect(TokType::KEYWORD, "if", "ключевое слово");
        auto n = makeNode(NodeKind::IfStmt);
        n->condition = parseExpression();
        n->thenBlock = parseBlock();
        if (matchExact(TokType::KEYWORD, "else")) {
            n->elseBlock = parseBlock();
        }
        return n;
    }

    NodePtr parseWhileStmt() {
        expect(TokType::KEYWORD, "while", "ключевое слово");
        auto n = makeNode(NodeKind::WhileStmt);
        n->condition = parseExpression();
        n->body = parseBlock();
        return n;
    }

    NodePtr parseForStmt() {
        expect(TokType::KEYWORD, "for", "ключевое слово");
        auto n = makeNode(NodeKind::ForStmt);

        if (checkType(TokType::IDENTIFIER)) {
            n->name = consume().value;
        }
        else {
            error("ожидалось имя счётчика цикла");
            n->name = "<error>";
        }
        expect(TokType::KEYWORD, "in", "ключевое слово");

        n->rangeStart = parseExpression();
        expect(TokType::OPERATOR, "..", "оператор диапазона");
        n->rangeEnd = parseExpression();
        n->body = parseBlock();
        return n;
    }

    NodePtr parseExpression() {
        NodePtr left = parseOperand();
        while (checkType(TokType::OPERATOR) && BIN_OPS.count(peek().value)) {
            std::string op = consume().value;
            NodePtr right = parseOperand();
            auto bin = makeNode(NodeKind::BinaryOp);
            bin->op = op;
            bin->left = left;
            bin->right = right;
            left = bin;
        }
        return left;
    }

    NodePtr parseOperand() {
        if (atEnd()) {
            error("неожиданный конец потока, ожидался операнд");
            return nullptr;
        }

        const Token& t = peek();

        if (t.type == TokType::CONSTANT_INT) {
            auto n = makeNode(NodeKind::Literal);
            n->litValue = consume().value; n->litKind = "int"; return n;
        }
        if (t.type == TokType::CONSTANT_FLOAT) {
            auto n = makeNode(NodeKind::Literal);
            n->litValue = consume().value; n->litKind = "float"; return n;
        }
        if (t.type == TokType::CONSTANT_STRING) {
            auto n = makeNode(NodeKind::Literal);
            n->litValue = consume().value; n->litKind = "string"; return n;
        }
        if (t.type == TokType::CONSTANT_BOOL) {
            auto n = makeNode(NodeKind::Literal);
            n->litValue = consume().value; n->litKind = "bool"; return n;
        }

        if (t.type == TokType::IDENTIFIER) {
            std::string name = consume().value;

            // макровызов: ИМЯ ! ( ... )
            if (checkExact(TokType::OPERATOR, "!") &&
                peek(1).type == TokType::DELIMITER && peek(1).value == "(") {
                pos++; // съели '!'
                auto call = makeNode(NodeKind::MacroCall);
                call->name = name;
                parseArgList(call->args);
                return call;
            }
            // обычный вызов функции: ИМЯ ( ... )
            if (checkExact(TokType::DELIMITER, "(")) {
                auto call = makeNode(NodeKind::CallExpr);
                call->name = name;
                parseArgList(call->args);
                return call;
            }
            // просто имя
            auto id = makeNode(NodeKind::Identifier);
            id->name = name;
            return id;
        }

        if (t.type == TokType::DELIMITER && t.value == "(") {
            pos++;
            auto e = parseExpression();
            expect(TokType::DELIMITER, ")", "закрывающая скобка");
            return e;
        }

        error("неожиданный токен '" + t.value + "', ожидался операнд");
        pos++; 
        return nullptr;
    }

    void parseArgList(std::vector<NodePtr>& out) {
        expect(TokType::DELIMITER, "(", "открывающая скобка");
        if (!checkExact(TokType::DELIMITER, ")")) {
            while (true) {
                auto e = parseExpression();
                if (e) out.push_back(e);
                if (matchExact(TokType::DELIMITER, ",")) continue;
                break;
            }
        }
        expect(TokType::DELIMITER, ")", "закрывающая скобка");
    }
};

namespace {
    std::string headerLine(const NodePtr& n) {
        if (!n) return "<null>";
        switch (n->kind) {
        case NodeKind::Program:      return "Program";
        case NodeKind::FunctionDecl: return "FunctionDecl";
        case NodeKind::ParamDecl:    return "ParamDecl";
        case NodeKind::Block:        return "Block";
        case NodeKind::VarDecl:      return "VarDecl";
        case NodeKind::AssignStmt:   return "AssignStmt";
        case NodeKind::IfStmt:       return "IfStmt";
        case NodeKind::WhileStmt:    return "WhileStmt";
        case NodeKind::ForStmt:      return "ForStmt";
        case NodeKind::CallExpr:     return "CallExpr";
        case NodeKind::MacroCall:    return "MacroCall";
        case NodeKind::BinaryOp:     return "BinaryOp(" + n->op + ")";
        case NodeKind::Identifier:   return "Identifier(" + n->name + ")";
        case NodeKind::Literal:      return "Literal(" + n->litValue + ", " + n->litKind + ")";
        }
        return "?";
    }

    bool isLeaf(const NodePtr& n) {
        return n && (n->kind == NodeKind::Identifier || n->kind == NodeKind::Literal);
    }

    struct Entry {
        enum Type { TEXT, CHILD, LIST } type;
        std::string label;
        std::string value;
        NodePtr child;
        std::vector<NodePtr> list;
    };

    std::vector<Entry> collectEntries(const NodePtr& n) {
        std::vector<Entry> v;
        auto T = [&](const std::string& l, const std::string& val) {
            Entry e; e.type = Entry::TEXT; e.label = l; e.value = val; v.push_back(e);
        };
        auto C = [&](const std::string& l, const NodePtr& c) {
            if (c) { Entry e; e.type = Entry::CHILD; e.label = l; e.child = c; v.push_back(e); }
        };
        auto L = [&](const std::string& l, const std::vector<NodePtr>& list) {
            if (list.empty()) return;       // пустые списки не рисуем
            Entry e; e.type = Entry::LIST; e.label = l; e.list = list; v.push_back(e);
        };

        switch (n->kind) {
        case NodeKind::Program:
            L("items", n->items);
            break;
        case NodeKind::FunctionDecl:
            T("name", n->name);
            L("params", n->params);
            if (n->hasReturnType) T("return_type", n->typeName);
            C("body", n->body);
            break;
        case NodeKind::ParamDecl:
            T("name", n->name);
            T("type", n->typeName);
            break;
        case NodeKind::Block:
            L("stmts", n->stmts);
            if (n->returnExpr) C("return_expr", n->returnExpr);
            break;
        case NodeKind::VarDecl:
            T("name", n->name);
            T("is_mutable", n->isMutable ? "true" : "false");
            T("type", n->typeName);
            C("init", n->init);
            break;
        case NodeKind::AssignStmt:
            C("left", n->left);
            C("right", n->right);
            break;
        case NodeKind::IfStmt:
            C("condition", n->condition);
            C("then_block", n->thenBlock);
            if (n->elseBlock) C("else_block", n->elseBlock);
            break;
        case NodeKind::WhileStmt:
            C("condition", n->condition);
            C("body", n->body);
            break;
        case NodeKind::ForStmt:
            T("var_name", n->name);
            C("range_start", n->rangeStart);
            C("range_end", n->rangeEnd);
            C("body", n->body);
            break;
        case NodeKind::CallExpr:
            T("callee", n->name);
            L("args", n->args);
            break;
        case NodeKind::MacroCall:
            T("name", n->name);
            L("args", n->args);
            break;
        case NodeKind::BinaryOp:
            C("left", n->left);
            C("right", n->right);
            break;
        case NodeKind::Identifier:
        case NodeKind::Literal:
            // У листьев нет вложенных полей.
            break;
        }
        return v;
    }

    void printBody(std::ostream& out, const NodePtr& n, const std::string& prefix);

    void printChildNode(std::ostream& out, const NodePtr& n,
        const std::string& prefix, bool isLast,
        const std::string& label) {
        std::string branch = isLast ? "└── " : "├── ";
        std::string sub = prefix + (isLast ? "    " : "│   ");

        out << prefix << branch;
        if (!label.empty()) out << label << ": ";
        out << headerLine(n) << "\n";

        if (!isLeaf(n)) printBody(out, n, sub);
    }

    void printBody(std::ostream& out, const NodePtr& n, const std::string& prefix) {
        auto entries = collectEntries(n);
        for (size_t i = 0; i < entries.size(); ++i) {
            bool last = (i + 1 == entries.size());
            std::string branch = last ? "└── " : "├── ";
            std::string sub = prefix + (last ? "    " : "│   ");
            const Entry& e = entries[i];

            if (e.type == Entry::TEXT) {
                out << prefix << branch << e.label << ": " << e.value << "\n";
            }
            else if (e.type == Entry::CHILD) {
                out << prefix << branch << e.label << ": " << headerLine(e.child) << "\n";
                if (!isLeaf(e.child)) printBody(out, e.child, sub);
            }
            else {
                out << prefix << branch << e.label << "\n";
                for (size_t k = 0; k < e.list.size(); ++k) {
                    bool kl = (k + 1 == e.list.size());
                    printChildNode(out, e.list[k], sub, kl, "");
                }
            }
        }
    }

}

ParseResult parse(const std::vector<Token>& tokens) {
    Parser p(tokens);
    ParseResult res;
    res.ast = p.parseProgram();
    res.errors = p.errors;
    return res;
}

void printAst(std::ostream& out, const NodePtr& root) {
    if (!root) { out << "<пустое дерево>\n"; return; }
    out << headerLine(root) << "\n";
    if (!isLeaf(root)) printBody(out, root, "");
}