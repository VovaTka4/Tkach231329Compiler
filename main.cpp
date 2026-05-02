#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>

#include "preprocessor.h"
#include "lexer.h"
#include "parser.h"

int main() {
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);
    setlocale(LC_ALL, "Russian");

    std::string inputPath, outputPath;

    std::cout << "Введите имя входного файла (исходный код): ";
    std::getline(std::cin, inputPath);
    std::cout << "Введите имя выходного файла (или Enter чтобы вывести на экран): ";
    std::getline(std::cin, outputPath);

    std::ifstream in(inputPath);
    if (!in) {
        std::cerr << "Ошибка: не удалось открыть файл " << inputPath << "\n";
        std::cin.get();
        return 1;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    std::string source = buf.str();

    // ===== ЛР1: препроцессор =====
    PreprocessResult pre = preprocess(source);
    if (!pre.ok()) {
        std::cerr << "Ошибки препроцессора:\n";
        for (auto& e : pre.errors) std::cerr << "  " << e << "\n";
        std::cin.get();
        return 1;
    }

    // ===== ЛР2: лексический анализатор =====
    LexResult lex = tokenize(pre.code);
    if (!lex.ok()) {
        std::cerr << "Лексические ошибки:\n";
        for (auto& e : lex.errors) std::cerr << "  " << e << "\n";
        std::cin.get();
        return 1;
    }

    // ==== ЛР3: синтаксический анализатор ====
    ParseResult par = parse(lex.tokens);
    std::ostringstream oss;

    oss << "Входные данные (поток токенов из ЛР2):\n[";
    for (size_t k = 0; k < lex.tokens.size(); ++k) {
        if (k) oss << ", ";
        oss << "(" << typeName(lex.tokens[k].type) << ", "
            << lex.tokens[k].value << ")";
    }
    oss << "]\n\n";

    oss << "Результат (AST):\n";
    printAst(oss, par.ast);
    oss << "\n";

    if (par.ok()) {
        oss << "Синтаксический анализ завершён успешно. Ошибок не найдено.\n";
    }
    else {
        oss << "Синтаксический анализ завершён с ошибками. Ошибок: "
            << par.errors.size() << "\n";
        for (auto& e : par.errors) oss << "  " << e << "\n";
    }

    if (!outputPath.empty()) {
        std::ofstream of(outputPath);
        if (!of) {
            std::cerr << "Ошибка: не удалось создать файл " << outputPath << "\n";
            std::cin.get();
            return 1;
        }
        of << oss.str();
        std::cerr << "Готово. Результат записан в " << outputPath << "\n";
    }
    else {
        std::cout << oss.str();
    }

    std::cin.get();
    return 0;
}
