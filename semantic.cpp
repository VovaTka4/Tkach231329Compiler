#include "semantic.h"
#include <map>
#include <sstream>
#include <iomanip>

namespace {

    struct Scope {
        std::string name;
        std::map<std::string, Symbol> table;
    };

    struct Sema {
        std::vector<Scope> scopes;          // стек областей видимости
        std::vector<Symbol> symbols;        // все объявленные символы по порядку
        std::vector<Triad> triads;          // итоговые триады
        std::vector<std::string> errors;    // семантические ошибки
        int labelCounter = 0;               // счётчик для генерации меток L1, L2, ...
        std::string currentFunc;            // имя анализируемой функции
        std::string currentReturnType;      // её тип возврата

        // Информация о результате выражения
        struct ExprInfo {
            std::string type; //тип выражения
            std::string operand; //как ссылаться на это значение в триаде (имя/литерал/^N)
        };

        void pushScope(const std::string& name) {
            scopes.push_back({ name, {} });
        }
        void popScope() { scopes.pop_back(); }

        // Поиск символа от вершины стека к глобальной области.
        Symbol* lookup(const std::string& name) {
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
                auto f = it->table.find(name);
                if (f != it->table.end()) return &f->second;
            }
            return nullptr;
        }

        // Объявить символ в текущей (верхней) области.
        // Если имя уже занято — выводим ошибку повторного объявления.
        void declare(const Symbol& s) {
            auto& top = scopes.back();
            if (top.table.count(s.name)) {
                errors.push_back(
                    "Семантическая ошибка: повторное объявление имени '" +
                    s.name + "' в области '" + top.name + "'");
                return;
            }
            top.table[s.name] = s;
            symbols.push_back(s);
        }

        // ----------- генерация триад и меток -----------
        std::string newLabel() {
            return "L" + std::to_string(++labelCounter);
        }

        int emit(const std::string& op,
            const std::string& a1,
            const std::string& a2)
        {
            Triad t;
            t.number = static_cast<int>(triads.size()) + 1;
            t.op = op; t.a1 = a1; t.a2 = a2;
            triads.push_back(t);
            return t.number;
        }

        // ----------- анализ выражений -----------
        ExprInfo analyzeExpr(const NodePtr& n);

        // ----------- анализ инструкций и блоков -----------
        void analyzeStmt(const NodePtr& n);
        void analyzeBlock(const NodePtr& n);
        void analyzeFunction(const NodePtr& fn);
        void analyzeProgram(const NodePtr& prog);
    };

    // Анализ выражений
    Sema::ExprInfo Sema::analyzeExpr(const NodePtr& n) {
        if (!n) return { "", "_" };

        switch (n->kind) {

        case NodeKind::Literal: {
            std::string t;
            if (n->litKind == "int")        t = "i32";
            else if (n->litKind == "float") t = "f64";
            else if (n->litKind == "bool")  t = "bool";
            else                            t = "string";
            return { t, n->litValue };
        }

        case NodeKind::Identifier: {
            Symbol* s = lookup(n->name);
            if (!s) {
                errors.push_back(
                    "Семантическая ошибка: использование необъявленной переменной '"
                    + n->name + "'");
                return { "<error>", n->name };
            }
            return { s->type, n->name };
        }

        case NodeKind::BinaryOp: {
            ExprInfo L = analyzeExpr(n->left);
            ExprInfo R = analyzeExpr(n->right);
            const std::string& op = n->op;
            std::string resultType = "<error>";

            if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
                if (L.type != R.type) {
                    errors.push_back(
                        "Семантическая ошибка: операция '" + op +
                        "' требует одинаковые числовые типы (получено " +
                        L.type + " и " + R.type + ")");
                }
                if (L.type != "i32" && L.type != "f64" && L.type != "<error>") {
                    errors.push_back(
                        "Семантическая ошибка: операция '" + op +
                        "' требует числовой тип (получено " + L.type + ")");
                }
                resultType = L.type;
            }
            else if (op == "==" || op == "!=" || op == "<" ||
                op == ">" || op == "<=" || op == ">=") {
                if (L.type != R.type) {
                    errors.push_back(
                        "Семантическая ошибка: операция сравнения '" + op +
                        "' требует одинаковые типы операндов (получено " +
                        L.type + " и " + R.type + ")");
                }
                resultType = "bool";
            }
            else if (op == "&&" || op == "||") {
                if (L.type != "bool" || R.type != "bool") {
                    errors.push_back(
                        "Семантическая ошибка: операция '" + op +
                        "' требует операнды типа bool (получено " +
                        L.type + " и " + R.type + ")");
                }
                resultType = "bool";
            }
            else {
                errors.push_back(
                    "Семантическая ошибка: неизвестная бинарная операция '" + op + "'");
            }

            int idx = emit(op, L.operand, R.operand);
            return { resultType, "^" + std::to_string(idx) };
        }

        case NodeKind::CallExpr: {
            Symbol* f = lookup(n->name);
            if (!f || f->category != "функция") {
                errors.push_back(
                    "Семантическая ошибка: вызов необъявленной функции '"
                    + n->name + "'");
            }

            std::vector<ExprInfo> args;
            for (auto& a : n->args) args.push_back(analyzeExpr(a));

            if (f && f->category == "функция") {
                if (args.size() != f->paramTypes.size()) {
                    errors.push_back(
                        "Семантическая ошибка: функция '" + n->name +
                        "' ожидает " + std::to_string(f->paramTypes.size()) +
                        " аргумент(ов), получено " + std::to_string(args.size()));
                }
                else {
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (args[i].type != f->paramTypes[i] &&
                            args[i].type != "<error>") {
                            errors.push_back(
                                "Семантическая ошибка: тип аргумента №" +
                                std::to_string(i + 1) + " вызова '" + n->name +
                                "': ожидается " + f->paramTypes[i] +
                                ", получено " + args[i].type);
                        }
                    }
                }
            }

            for (auto& a : args) emit("param", a.operand, "_");
            int idx = emit("call", n->name, std::to_string(args.size()));

            std::string rt = f ? f->returnType : "<error>";
            return { rt, "^" + std::to_string(idx) };
        }

        case NodeKind::MacroCall: {
            std::vector<ExprInfo> args;
            for (auto& a : n->args) args.push_back(analyzeExpr(a));
            for (auto& a : args) emit("param", a.operand, "_");
            int idx = emit("call", n->name + "!", std::to_string(args.size()));
            return { "()", "^" + std::to_string(idx) };
        }

        default:
            return { "<error>", "_" };
        }
    }
    // Анализ инструкций
    void Sema::analyzeStmt(const NodePtr& n) {
        if (!n) return;

        switch (n->kind) {
        case NodeKind::VarDecl: {
            ExprInfo init = analyzeExpr(n->init);
            if (init.type != n->typeName && init.type != "<error>") {
                errors.push_back(
                    "Семантическая ошибка: тип выражения-инициализатора '" +
                    init.type + "' не совпадает с типом переменной '" +
                    n->name + "' (" + n->typeName + ")");
            }
            Symbol s;
            s.name = n->name;
            s.category = "переменная";
            s.type = n->typeName;
            s.scope = scopes.back().name;
            s.isMutable = n->isMutable;
            s.declared = true;
            s.initialized = (n->init != nullptr);
            declare(s);
            emit(":=", n->name, init.operand);
            break;
        }

        case NodeKind::AssignStmt: {
            std::string lhsName = n->left ? n->left->name : "<error>";
            Symbol* sym = lookup(lhsName);

            if (!sym) {
                errors.push_back(
                    "Семантическая ошибка: присваивание необъявленной переменной '"
                    + lhsName + "'");
            }
            else {
                if (sym->category == "параметр") {
                    errors.push_back(
                        "Семантическая ошибка: нельзя присваивать значение параметру функции '"
                        + lhsName + "'");
                }
                else if (!sym->isMutable) {
                    errors.push_back(
                        "Семантическая ошибка: присваивание неизменяемой переменной '"
                        + lhsName + "' (объявите её через 'let mut')");
                }
            }

            ExprInfo rhs = analyzeExpr(n->right);
            if (sym && rhs.type != sym->type && rhs.type != "<error>") {
                errors.push_back(
                    "Семантическая ошибка: тип правой части '" + rhs.type +
                    "' не совпадает с типом переменной '" + lhsName +
                    "' (" + sym->type + ")");
            }
            if (sym) sym->initialized = true;
            emit(":=", lhsName, rhs.operand);
            break;
        }

        case NodeKind::IfStmt: {
            ExprInfo cond = analyzeExpr(n->condition);
            if (cond.type != "bool" && cond.type != "<error>") {
                errors.push_back(
                    "Семантическая ошибка: условие if должно иметь тип bool (получено "
                    + cond.type + ")");
            }
            std::string Lelse = newLabel();
            std::string Lend = newLabel();

            emit("jumpcheck", cond.operand, Lelse);

            pushScope(scopes.back().name + "::if-then");
            analyzeBlock(n->thenBlock);
            popScope();

            emit("jump", Lend, "_");

            emit("label", Lelse, "_");
            if (n->elseBlock) {
                pushScope(scopes.back().name + "::if-else");
                analyzeBlock(n->elseBlock);
                popScope();
            }
            emit("label", Lend, "_");
            break;
        }

        case NodeKind::WhileStmt: {
            std::string Lstart = newLabel();
            std::string Lend = newLabel();

            emit("label", Lstart, "_");
            ExprInfo cond = analyzeExpr(n->condition);
            if (cond.type != "bool" && cond.type != "<error>") {
                errors.push_back(
                    "Семантическая ошибка: условие while должно иметь тип bool (получено "
                    + cond.type + ")");
            }
            emit("jumpcheck", cond.operand, Lend);

            pushScope(scopes.back().name + "::while");
            analyzeBlock(n->body);
            popScope();

            emit("jump", Lstart, "_");
            emit("label", Lend, "_");
            break;
        }

        case NodeKind::ForStmt: {
            ExprInfo a = analyzeExpr(n->rangeStart);
            ExprInfo b = analyzeExpr(n->rangeEnd);
            std::string itType = a.type;
            if (a.type != b.type) {
                errors.push_back(
                    "Семантическая ошибка: границы диапазона for должны иметь одинаковый тип");
            }
            if (itType != "i32" && itType != "<error>") {
                errors.push_back(
                    "Семантическая ошибка: границы диапазона for должны быть целочисленными ("
                    + itType + ")");
                itType = "i32";
            }

            int rngIdx = emit("range", a.operand, b.operand);

            pushScope("for-блок в " + currentFunc);

            Symbol s;
            s.name = n->name;
            s.category = "переменная";
            s.type = itType;
            s.scope = scopes.back().name;
            s.isMutable = false;
            s.declared = true;
            s.initialized = true;
            declare(s);

            emit(":=", n->name, "^" + std::to_string(rngIdx) + ".first");

            std::string Lstart = newLabel();
            std::string Lend = newLabel();
            emit("label", Lstart, "_");

            int condIdx = emit("<", n->name, "^" + std::to_string(rngIdx) + ".second");
            emit("jumpcheck", "^" + std::to_string(condIdx), Lend);

            analyzeBlock(n->body);

            int incIdx = emit("+", n->name, "1");
            emit(":=", n->name, "^" + std::to_string(incIdx));
            emit("jump", Lstart, "_");
            emit("label", Lend, "_");

            popScope();
            break;
        }

        default:
            analyzeExpr(n);
            break;
        }
    }

    // Анализ блока { ... }
    void Sema::analyzeBlock(const NodePtr& blk) {
        if (!blk) return;
        for (auto& s : blk->stmts) analyzeStmt(s);

        if (blk->returnExpr) {
            ExprInfo r = analyzeExpr(blk->returnExpr);
            if (currentReturnType != "()" &&
                r.type != currentReturnType &&
                r.type != "<error>") {
                errors.push_back(
                    "Семантическая ошибка: тип возвращаемого значения '" + r.type +
                    "' не совпадает с типом возврата функции '" + currentFunc +
                    "' (" + currentReturnType + ")");
            }
            emit("return", r.operand, "_");
        }
    }

    // Анализ объявления функции
    void Sema::analyzeFunction(const NodePtr& fn) {
        currentFunc = fn->name;
        currentReturnType = fn->hasReturnType ? fn->typeName : "()";

        pushScope(fn->name);
        for (auto& p : fn->params) {
            Symbol s;
            s.name = p->name;
            s.category = "параметр";
            s.type = p->typeName;
            s.scope = fn->name;
            s.isMutable = false;
            s.declared = true;
            s.initialized = true;
            declare(s);
        }
        analyzeBlock(fn->body);
        popScope();
    }

    // Анализ всей программы
    void Sema::analyzeProgram(const NodePtr& prog) {
        if (!prog) return;
        pushScope("global");

        for (auto& fn : prog->items) {
            if (!fn || fn->kind != NodeKind::FunctionDecl) continue;

            Symbol s;
            s.name = fn->name;
            s.category = "функция";
            s.scope = "global";
            s.declared = true;
            s.initialized = true;
            s.returnType = fn->hasReturnType ? fn->typeName : "()";
            for (auto& p : fn->params) s.paramTypes.push_back(p->typeName);

            // Текстовое представление сигнатуры — для таблицы.
            std::string sig = "(";
            for (size_t i = 0; i < s.paramTypes.size(); ++i) {
                if (i) sig += ", ";
                sig += s.paramTypes[i];
            }
            sig += ") -> " + s.returnType;
            s.type = sig;

            declare(s);
        }

        for (auto& fn : prog->items) {
            if (fn && fn->kind == NodeKind::FunctionDecl) {
                analyzeFunction(fn);
            }
        }
        popScope();
    }

}

SemanticResult analyze(const NodePtr& ast) {
    Sema s;
    s.analyzeProgram(ast);

    SemanticResult r;
    r.symbols = s.symbols;
    r.triads = s.triads;
    r.errors = s.errors;
    return r;
}

// Печать таблицы символов
void printSymbolTable(std::ostream& out, const std::vector<Symbol>& syms) {
    out << "Таблица символов:\n";
    out << "+----+--------+------------+----------------------+-----------------+-----+----------+----------------+\n";
    out << "| №  | Имя    | Категория  | Тип                  | Область         | Mut | Объявлена| Инициализир.   |\n";
    out << "+----+--------+------------+----------------------+-----------------+-----+----------+----------------+\n";

    auto pad = [](std::string s, size_t w) {
        if (s.size() < w) s.append(w - s.size(), ' ');
        else if (s.size() > w) { s = s.substr(0, w); }
        return s;
    };

    for (size_t i = 0; i < syms.size(); ++i) {
        const Symbol& s = syms[i];
        out << "| " << pad(std::to_string(i + 1), 2)
            << " | " << pad(s.name, 6)
            << " | " << pad(s.category, 10)
            << " | " << pad(s.type, 20)
            << " | " << pad(s.scope, 15)
            << " | " << pad(s.isMutable ? "+" : "-", 3)
            << " | " << pad(s.declared ? "+" : "-", 8)
            << " | " << pad(s.initialized ? "+" : "-", 14)
            << " |\n";
    }
    out << "+----+--------+------------+----------------------+-----------------+-----+----------+----------------+\n";
}

// Печать триад
void printTriads(std::ostream& out, const std::vector<Triad>& triads) {
    out << "Промежуточное представление (триады):\n";
    for (const auto& t : triads) {
        out << t.number << ") (" << t.op << ", " << t.a1 << ", " << t.a2 << ")\n";
    }
}