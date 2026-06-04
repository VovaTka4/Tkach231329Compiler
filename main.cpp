#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>

#include "preprocessor.h"   // ЛР1
#include "lexer.h"          // ЛР2
#include "parser.h"         // ЛР3
#include "semantic.h"       // ЛР4

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
    std::ostringstream oss;

    // ===== ЛР1: препроцессор =====
    PreprocessResult pre = preprocess(source);
    if (!pre.ok()) {
        std::cerr << "Ошибки препроцессора:\n";
        for (auto& e : pre.errors) std::cerr << "  " << e << "\n";
        std::cin.get();
        return 1;
    }
    oss << "ЛР1 ПРЕПРОЦЕССИНГ\n";
    oss << "Препроцесинг завершен\n";
    oss << "Очищенный код: \n" << pre.code << "\n";
    // ===== ЛР2: лексический анализатор =====
    LexResult lex = tokenize(pre.code);
    if (!lex.ok()) {
        std::cerr << "Лексические ошибки:\n";
        for (auto& e : lex.errors) std::cerr << "  " << e << "\n";
        std::cin.get();
        return 1;
    }
    oss << "ЛР2\n";
    oss << "Токенов распознано: " << lex.tokens.size() << "\n";
    for (auto& t : lex.tokens) {
        oss << "  [" << typeName(t.type) << "] " << t.value
            << "  (стр. " << t.line << ", кол. " << t.col << ")\n";
    }
    oss << "\n";

    // ===== ЛР3: синтаксический анализатор =====
    ParseResult par = parse(lex.tokens);
    oss << "ЛР3\n";

    if (!par.ok()) {
        oss << "Синтаксический анализ завершён с ошибками. Ошибок: "
            << par.errors.size() << "\n";
        for (auto& e : par.errors) oss << "  " << e << "\n";
        // С невалидным AST смысла идти в семантический анализ нет.
        if (!outputPath.empty()) {
            std::ofstream of(outputPath);
            of << oss.str();
        }
        else {
            std::cout << oss.str();
        }
        std::cin.get();
        return 1;
    }
    else {
        oss << "Синтаксический анализ завершён успешно. Ошибок не найдено.\n\n";
        oss << "Дерево-AST:\n";
        printAst(oss, par.ast);
        oss << "\n";
    }

    // ===== ЛР4: семантический анализ + промежуточное представление =====
    SemanticResult sem = analyze(par.ast);
    oss << "ЛР4\n";
    printSymbolTable(oss, sem.symbols);
    oss << "\n";

    if (sem.ok()) {
        oss << "Семантический анализ завершён успешно. Ошибок не найдено.\n\n";
    }
    else {
        oss << "Семантический анализ завершён с ошибками. Ошибок: "
            << sem.errors.size() << "\n";
        for (auto& e : sem.errors) oss << "  " << e << "\n";
        oss << "\n";
    }

    printTriads(oss, sem.triads);

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