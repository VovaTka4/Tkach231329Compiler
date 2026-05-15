#pragma once
#include "parser.h"
#include <string>
#include <vector>
#include <ostream>

// Один символ в таблице символов.
// Описывает функцию, параметр или переменную.
struct Symbol {
    std::string name;          // имя символа
    std::string category;      // "функция" / "параметр" / "переменная"
    std::string type;          // тип: i32 / bool / "(i32, i32) -> i32" и т.п.
    std::string scope;         // имя области видимости (global / main / for-блок в main / ...)
    bool isMutable = false;    // признак mut
    bool declared = true;      // объявлен (всегда true, оставлено для наглядности таблицы)
    bool initialized = false;  // признак инициализации

    // Поля для функций:
    std::vector<std::string> paramTypes; // типы параметров по порядку
    std::string returnType;              // тип возвращаемого значения ("()" если нет ->)
};

// Триада промежуточного представления.
struct Triad {
    int number;        // порядковый номер 
    std::string op;    // имя операции 
    std::string a1;    // первый операнд
    std::string a2;    // второй операнд (или "_")
};

// Результат работы семантического анализатора.
struct SemanticResult {
    std::vector<Symbol> symbols;          // итоговая таблица символов (в порядке объявления)
    std::vector<Triad> triads;            // последовательность триад
    std::vector<std::string> errors;      // список семантических ошибок
    bool ok() const { return errors.empty(); }
};

// Главная точка входа: запустить анализ на готовом AST.
SemanticResult analyze(const NodePtr& ast);

// Красивый вывод таблицы символов и триад.
void printSymbolTable(std::ostream& out, const std::vector<Symbol>& syms);
void printTriads(std::ostream& out, const std::vector<Triad>& triads);