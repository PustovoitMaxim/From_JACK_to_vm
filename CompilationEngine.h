#pragma once
#include "JackTokenizer.h"
#include "SymbolTable.h"
#include "VMWriter.h"
#include <string>
#include <memory>



class CompilationEngine {
public:
    CompilationEngine(JackTokenizer& tokenizer,
        VMWriter& vmWriter,
        SymbolTable& symbolTable,
        const std::string& className);

    // Основные методы компиляции
    void compileClass();
    void compileClassVarDec();
    void compileSubroutine();
    void compileParameterList();
    void compileVarDec();
    void compileStatements();
    void compileLet();
    void compileIf();
    void compileWhile();
    void compileDo();
    void compileReturn();
    void compileExpression();
    void compileTerm();
    void compileExpressionList();
    void compileSubroutineCall(const std::string& identifier);

private:
    // Вспомогательные методы

    void expect(const std::string& expected);
    void consumeSymbol(std::string symbol);
    void consumeKeyword(Keyword keyword);
    void eat();

    // Преобразует VarKind в VM-сегмент

    std::string kindToSegment(VarKind kind) const;

    // Генерация уникальных меток

    std::string generateLabel(const std::string& prefix);
    bool isOperator(std::string c) const;
    bool isUnaryOp() const;
    bool isBuiltInClass(const std::string& className) const;
    void emitOperator(const std::string& op);

    

    // Состояние
    JackTokenizer& tokenizer;
    VMWriter& vmWriter;
    SymbolTable& symbolTable;
    std::string className;
    std::string currentSubroutine;
    int labelCounter = 0;
    int currentExpressionCount = 0; // Для хранения количества аргументов
};