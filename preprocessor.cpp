#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <windows.h>

int main() {
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);
    setlocale(LC_ALL, "Russian");

    std::string inputPath, outputPath;

    std::cout << "Введите имя входного файла: ";
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
    std::string code = buf.str();

    // 1. Проверка недопустимых символов (управляющие байты < 0x20, кроме \n, \r, \t).
    for (size_t i = 0; i < code.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(code[i]);
        if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
            std::cerr << "Ошибка: недопустимый символ с кодом " << static_cast<int>(c) << " в позиции " << i << "\n";
            std::cin.get();
            return 1;
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
        std::cerr << "Ошибка: незакрытый многострочный комментарий в позиции " << first_open << "\n";
        std::cin.get();
        return 1;
    }
    else if (closers > 0) {
        std::cerr << "Ошибка: незакрытый многострочный комментарий в позиции " << first_close << "\n";
        std::cin.get();
        return 1;
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

    if (!outputPath.empty()) {
        std::ofstream outf(outputPath);
        if (!outf) {
            std::cerr << "Ошибка: не удалось создать файл " << outputPath << "\n";
            std::cin.get();
            return 1;
        }
        outf << out;
        std::cerr << "Готово. Результат записан в " << outputPath << "\n";
    }
    else {
        std::cout << out;
    }

    std::cerr << "Ошибок не выявлено\n";
    std::cin.get();
    return 0;
}