#pragma once
#include <string>
#include <vector>

// –езультат работы препроцессора (Ћ–1).
// code  Ч очищенный код (без комментариев, лишних пробелов и пустых строк);
// errors Ч непустой, если были обнаружены ошибки (тогда code пустой).
struct PreprocessResult {
    std::string code;
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

PreprocessResult preprocess(const std::string& source);
