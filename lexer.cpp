#include "lexer.h"
#include <set>

// ---- таблицы лексем ----
static const std::set<std::string> KEYWORDS = {
    "fn", "let", "mut", "if", "else", "while", "for", "in", "return",
    "i32", "f64", "bool"
};

static const std::set<std::string> OPERATORS = {
    "+", "-", "*", "/", "%",
    "=", "==", "!=", "<", ">", "<=", ">=",
    "&&", "||", "!",
    "->", "..",
    "+=", "-=", "*=", "/="
};

static const std::set<char> DELIMITERS = {
    '(', ')', '{', '}', '[', ']', ',', ';', ':', '.'
};

std::string typeName(TokType t) {
    switch (t) {
    case TokType::KEYWORD:         return "KEYWORD";
    case TokType::IDENTIFIER:      return "IDENTIFIER";
    case TokType::CONSTANT_INT:    return "CONSTANT_INT";
    case TokType::CONSTANT_FLOAT:  return "CONSTANT_FLOAT";
    case TokType::CONSTANT_STRING: return "CONSTANT_STRING";
    case TokType::CONSTANT_BOOL:   return "CONSTANT_BOOL";
    case TokType::OPERATOR:        return "OPERATOR";
    case TokType::DELIMITER:       return "DELIMITER";
    }
    return "?";
}

// множество символов, из которых состоят операторы
static bool isOpChar(char c) {
    static const std::string opChars = "+-*/%=!<>&|^.?";
    return opChars.find(c) != std::string::npos;
}

static bool isIdStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static bool isIdPart(char c) {
    return isIdStart(c) || (c >= '0' && c <= '9');
}
static bool isDigitCh(char c) {
    return c >= '0' && c <= '9'; 
}

LexResult tokenize(const std::string& code) {
    LexResult res;
    size_t i = 0;
    size_t line = 1, col = 1;

    auto advance = [&](size_t n = 1) {
        for (size_t k = 0; k < n && i < code.size(); ++k) {
            if (code[i] == '\n') { 
                line++; col = 1; 
            }
            else 
                col++;
            i++;
        }
    };

    while (i < code.size()) {
        unsigned char uc = static_cast<unsigned char>(code[i]);
        char c = code[i];

        // пробельные символы
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
            continue;
        }

        size_t startLine = line, startCol = col;

        // строковый литерал "..."
        if (c == '"') {
            advance();
            std::string s = "\"";
            bool closed = false;
            while (i < code.size()) {
                if (code[i] == '\\' && i + 1 < code.size()) {
                    s += code[i];
                    s += code[i + 1];
                    advance(2);
                    continue;
                }
                if (code[i] == '"') {
                    s += '"';
                    advance();
                    closed = true;
                    break;
                }
                if (code[i] == '\n') 
                    break;
                s += code[i];
                advance();
            }
            if (!closed) {
                res.errors.push_back(
                    "Ошибка [строка " + std::to_string(startLine) +
                    ", столбец " + std::to_string(startCol) +
                    "]: незакрытый строковый литерал");
            }
            else {
                res.tokens.push_back({ TokType::CONSTANT_STRING, s, startLine, startCol });
            }
            continue;
        }

        // идентификатор / ключевое слово / булев литерал
        if (isIdStart(c)) {
            std::string s;
            while (i < code.size() && isIdPart(code[i])) {
                s += code[i];
                advance();
            }
            if (s == "true" || s == "false") {
                res.tokens.push_back({ TokType::CONSTANT_BOOL, s, startLine, startCol });
            }
            else if (KEYWORDS.count(s)) {
                res.tokens.push_back({ TokType::KEYWORD, s, startLine, startCol });
            }
            else {
                res.tokens.push_back({ TokType::IDENTIFIER, s, startLine, startCol });
            }
            continue;
        }

        // числовая константа
        if (isDigitCh(c)) {
            std::string s;
            while (i < code.size() && isDigitCh(code[i])) {
                s += code[i];
                advance();
            }
            bool isFloat = false;

            // точка как часть дроби, НО не ".." (оператор диапазона)
            if (i + 1 < code.size() && code[i] == '.' &&
                code[i + 1] != '.' && isDigitCh(code[i + 1])) {
                isFloat = true;
                s += '.';
                advance();
                while (i < code.size() && isDigitCh(code[i])) {
                    s += code[i];
                    advance();
                }
                // ещё одна точка подряд — ошибка
                if (i < code.size() && code[i] == '.' &&
                    (i + 1 >= code.size() || code[i + 1] != '.')) {
                    std::string bad = s;
                    while (i < code.size() && (isDigitCh(code[i]) || code[i] == '.')) {
                        bad += code[i];
                        advance();
                    }
                    res.errors.push_back(
                        "Ошибка [строка " + std::to_string(startLine) +
                        "]: некорректная вещественная константа '" + bad + "' (две точки подряд)");
                    continue;
                }
            }

            // буква сразу за числом — ошибка
            if (i < code.size() && (isIdStart(code[i]) ||
                static_cast<unsigned char>(code[i]) >= 0x80)) {
                std::string bad = s;
                while (i < code.size() && (isIdPart(code[i]) ||
                    static_cast<unsigned char>(code[i]) >= 0x80)) {
                    bad += code[i];
                    advance();
                }
                res.errors.push_back(
                    "Ошибка [строка " + std::to_string(startLine) +
                    "]: некорректная числовая константа '" + bad +
                    "' (недопустимые символы в числовом литерале)");
                continue;
            }

            res.tokens.push_back({
                isFloat ? TokType::CONSTANT_FLOAT : TokType::CONSTANT_INT,
                s, startLine, startCol });
            continue;
        }

        // проверка на известные неверные операторы (до обычного разбора)
        if (i + 2 < code.size()) {
            std::string three;
            three += code[i];
            three += code[i + 1];
            three += code[i + 2];
            static const std::set<std::string> BAD_OPS3 = {
                "===", "!==", "<<<", ">>>", "+++", "---", "***"
            };
            if (BAD_OPS3.count(three)) {
                res.errors.push_back(
                    "Ошибка [строка " + std::to_string(startLine) +
                    ", столбец " + std::to_string(startCol) +
                    "]: неизвестный оператор '" + three + "'");
                advance(3);
                continue;
            }
        }
        if (i + 1 < code.size()) {
            std::string two;
            two += code[i];
            two += code[i + 1];
            static const std::set<std::string> BAD_OPS2 = {
                "<>", "><", "=<", "=>", "**", "??"
            };
            if (BAD_OPS2.count(two)) {
                res.errors.push_back(
                    "Ошибка [строка " + std::to_string(startLine) +
                    ", столбец " + std::to_string(startCol) +
                    "]: неизвестный оператор '" + two + "'");
                advance(2);
                continue;
            }
            if (OPERATORS.count(two)) {
                res.tokens.push_back({ TokType::OPERATOR, two, startLine, startCol });
                advance(2);
                continue;
            }
        }

        // односимвольный оператор
        {
            std::string one(1, c);
            if (OPERATORS.count(one)) {
                res.tokens.push_back({ TokType::OPERATOR, one, startLine, startCol });
                advance();
                continue;
            }
        }

        // разделитель
        if (DELIMITERS.count(c)) {
            res.tokens.push_back({ TokType::DELIMITER, std::string(1, c), startLine, startCol });
            advance();
            continue;
        }

        // недопустимый символ
        std::string shown;
        if (uc >= 0x20 && uc < 0x80) shown = std::string(1, c);
        else {
            char b[8];
            snprintf(b, sizeof(b), "0x%02X", uc);
            shown = b;
        }
        res.errors.push_back(
            "Ошибка [строка " + std::to_string(startLine) +
            ", столбец " + std::to_string(startCol) +
            "]: недопустимый символ '" + shown + "'");
        advance();
    }

    return res;
}