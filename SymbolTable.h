#pragma once
#include <string>
#include <unordered_map>

enum class VarKind { STATIC, FIELD, ARG, VAR, NONE };

class SymbolTable {
public:
    SymbolTable();

    void defineMethod(const std::string& methodName, const std::string& returnType);

    std::string getMethodReturnType(const std::string& methodName) const;

    // Начать новую подпрограмму (сбрасывает таблицу ARG и VAR)
    void startSubroutine();

    // Добавить переменную в таблицу
    void define(const std::string& name, const std::string& type, VarKind kind);

    // Количество переменных заданного вида
    int varCount(VarKind kind) const;

    // Получить информацию о переменной
    VarKind kindOf(const std::string& name) const;
    std::string typeOf(const std::string& name) const;
    int indexOf(const std::string& name) const;

private:
    struct Symbol {
        std::string type;
        VarKind kind;
        int index;
    };

    // Таблицы символов
    std::unordered_map<std::string, Symbol> classTable;      // STATIC, FIELD
    std::unordered_map<std::string, Symbol> subroutineTable; // ARG, VAR
    std::unordered_map<std::string, std::string> methodReturnTypes; // methodName → returnType
    // Счетчики переменных
    int staticCount;
    int fieldCount;
    int argCount;
    int varCount_;
};