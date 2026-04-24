#include "preprocessor.h"
#include <regex>
#include <sstream>

PreprocessResult preprocess(const std::string& source) {
    PreprocessResult res;
    std::string code = source;

    // 1. Проверка недопустимых символов (управляющие байты < 0x20, кроме \n, \r, \t).
    for (size_t i = 0; i < code.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(code[i]);
        if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
            res.errors.push_back(
                "недопустимый символ с кодом " + std::to_string(static_cast<int>(c)) +
                " в позиции " + std::to_string(i));
            return res;
        }
    }

    // 2. Проверка незакрытого многострочного комментария /* ... */
    int openers = 0;
    int closers = 0;
    size_t first_open = 0;
    size_t first_close = 0;
    size_t i = 0;
    while (i + 1 < code.size()) {
        if (code[i] == '/' && code[i + 1] == '*') {
            if (openers == 0) first_open = i;
            openers++;
            i += 2;
        }
        else if (code[i] == '*' && code[i + 1] == '/') {
            if (openers > 0) {
                openers--;
                i += 2;
            }
            else {
                closers++;
                first_close = i;
                i += 2;
            }
        }
        else {
            i++;
        }
    }
    if (openers > 0) {
        res.errors.push_back(
            "незакрытый многострочный комментарий в позиции " + std::to_string(first_open));
        return res;
    }
    else if (closers > 0) {
        res.errors.push_back(
            "незакрытый многострочный комментарий в позиции " + std::to_string(first_close));
        return res;
    }

    // 3. Удаление многострочных комментариев /* ... */
    std::regex block_re(R"(/\*[\s\S]*?\*/)");
    code = std::regex_replace(code, block_re, "");

    // 4. Удаление однострочных комментариев // ...
    std::regex line_re(R"(//[^\n]*)");
    code = std::regex_replace(code, line_re, "");

    // 5. Обработка построчно: обрезать края, схлопнуть пробелы, удалить пустые строки.
    std::regex trim_edges(R"(^[ \t]+|[ \t]+$)");
    std::regex multi_space(R"([ \t]{2,})");
    std::stringstream ss(code);
    std::string line, out;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = std::regex_replace(line, trim_edges, "");
        line = std::regex_replace(line, multi_space, " ");
        if (!line.empty()) {
            out += line;
            out += '\n';
        }
    }

    res.code = out;
    return res;
}