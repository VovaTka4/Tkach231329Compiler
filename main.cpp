#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>

#include "preprocessor.h"
#include "lexer.h"

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

    // ===== формирование отчёта =====
    std::ostringstream oss;

    oss << inputPath << " (очищенный результат ЛР1)\n";
    oss << pre.code;
    if (pre.code.empty() || pre.code.back() != '\n') oss << "\n";

    oss << "\nРезультат\n";
    oss << "Лексема              | Тип\n";
    oss << "---------------------+--------------------\n";
    for (auto& t : lex.tokens) {
        std::string v = t.value;
        if (v.size() < 20) v += std::string(20 - v.size(), ' ');
        oss << v << " | " << typeName(t.type) << "\n";
    }

    oss << "\n[";
    for (size_t k = 0; k < lex.tokens.size(); ++k) {
        if (k) oss << ", ";
        oss << "(" << typeName(lex.tokens[k].type) << ", " << lex.tokens[k].value << ")";
    }
    oss << "]\n\n";

    if (lex.ok()) {
        oss << "Лексический анализ завершён успешно. Обнаружено "
            << lex.tokens.size() << " токенов. Ошибок не найдено.\n";
    }
    else {
        oss << "Лексический анализ завершён с ошибками. Токенов: "
            << lex.tokens.size() << ". Ошибок: " << lex.errors.size() << "\n";
        for (auto& e : lex.errors) oss << "  " << e << "\n";
    }

    if (!outputPath.empty()) {
        std::ofstream outf(outputPath);
        if (!outf) {
            std::cerr << "Ошибка: не удалось создать файл " << outputPath << "\n";
            std::cin.get();
            return 1;
        }
        outf << oss.str();
        std::cerr << "Готово. Результат записан в " << outputPath << "\n";
    }
    else {
        std::cout << oss.str();
    }

    std::cin.get();
    return 0;
}
