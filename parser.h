#pragma once

#include "lexer.h"
#include <memory>
#include <string>
#include <vector>
#include <ostream>

// Виды узлов AST.
enum class NodeKind {
    Program,        // вся программа (корень)
    FunctionDecl,   // объявление функции:  fn ИМЯ(...) [-> тип] { ... }
    ParamDecl,      // один параметр функции: имя : тип
    Block,          // блок { ... } (тело функции, цикла, ветки if/else)
    VarDecl,        // объявление переменной: let [mut] имя : тип = выражение;
    AssignStmt,     // присваивание: имя = выражение;
    IfStmt,         // условный оператор if/else
    WhileStmt,      // цикл while
    ForStmt,        // цикл for имя in от..до
    CallExpr,       // вызов функции: имя(аргументы)
    MacroCall,      // вызов макроса:  имя!(аргументы)
    BinaryOp,       // бинарная операция (+, -, ==, &&, ...)
    Identifier,     // имя (используется как операнд в выражении)
    Literal         // литеральное значение (число/строка/булево)
};

struct Node;
using NodePtr = std::shared_ptr<Node>;

struct Node {
    NodeKind kind;

    std::string name;            
    std::string typeName;        
    std::string op;              
    std::string litValue;       
    std::string litKind;         
    bool isMutable = false;      
    bool hasReturnType = false; 

    std::vector<NodePtr> items;   
    std::vector<NodePtr> params;  
    std::vector<NodePtr> stmts;   
    std::vector<NodePtr> args;    

    NodePtr body;         
    NodePtr returnExpr;   
    NodePtr init;         
    NodePtr left;         
    NodePtr right;        
    NodePtr condition;    
    NodePtr thenBlock;    
    NodePtr elseBlock;    
    NodePtr rangeStart;   
    NodePtr rangeEnd;     
};

struct ParseResult {
    NodePtr ast;                          
    std::vector<std::string> errors;      
    bool ok() const { return errors.empty(); }
};

ParseResult parse(const std::vector<Token>& tokens);

void printAst(std::ostream& out, const NodePtr& root);