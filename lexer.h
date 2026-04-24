#pragma once
#include <string>
#include <vector>

enum class TokType {
    KEYWORD, IDENTIFIER,
    CONSTANT_INT, CONSTANT_FLOAT, CONSTANT_STRING, CONSTANT_BOOL,
    OPERATOR, DELIMITER
};

std::string typeName(TokType t);

// “окен Ч тот самый Ђобъект с двум€ пол€миї, передаваемый синтаксическому анализатору.
// line/col Ч дл€ диагностики ошибок.
struct Token {
    TokType type;
    std::string value;
    size_t line;
    size_t col;
};

struct LexResult {
    std::vector<Token> tokens;
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

LexResult tokenize(const std::string& code);
