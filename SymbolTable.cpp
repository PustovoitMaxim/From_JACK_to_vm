#include "SymbolTable.h"
#include <stdexcept>

SymbolTable::SymbolTable()
    : staticCount(0), fieldCount(0), argCount(0), varCount_(0) {}

void SymbolTable::startSubroutine() {
    subroutineTable.clear();
    argCount = 0;
    varCount_ = 0;
}

void SymbolTable::defineMethod(const std::string& methodName, const std::string& returnType) {
    methodReturnTypes[methodName] = returnType;
}

std::string SymbolTable::getMethodReturnType(const std::string& methodName) const {
    auto it = methodReturnTypes.find(methodName);
    return (it != methodReturnTypes.end()) ? it->second : "unknown";
}

void SymbolTable::define(
    const std::string& name,
    const std::string& type,
    VarKind kind
) {
    switch (kind) {
    case VarKind::STATIC:
        classTable[name] = { type, kind, staticCount++ };
        break;
    case VarKind::FIELD:
        classTable[name] = { type, kind, fieldCount++ };
        break;
    case VarKind::ARG:
        subroutineTable[name] = { type, kind, argCount++ };
        break;
    case VarKind::VAR:
        subroutineTable[name] = { type, kind, varCount_++ };
        break;
    default:
        throw std::runtime_error("Invalid variable kind");
    }
}

int SymbolTable::varCount(VarKind kind) const {
    switch (kind) {
    case VarKind::STATIC: return staticCount;
    case VarKind::FIELD:  return fieldCount;
    case VarKind::ARG:    return argCount;
    case VarKind::VAR:    return varCount_;
    default: return 0;
    }
}

VarKind SymbolTable::kindOf(const std::string& name) const {
    // Сначала проверяем локальные переменные и аргументы
    auto it = subroutineTable.find(name);
    if (it != subroutineTable.end()) {
        return it->second.kind;
    }

    // Затем проверяем статические и поля класса
    it = classTable.find(name);
    if (it != classTable.end()) {
        return it->second.kind;
    }

    return VarKind::NONE;
}

std::string SymbolTable::typeOf(const std::string& name) const {
    // Сначала проверяем подпрограмму
    auto it = subroutineTable.find(name);
    if (it != subroutineTable.end()) {
        return it->second.type;
    }

    // Затем проверяем класс
    it = classTable.find(name);
    if (it != classTable.end()) {
        return it->second.type;
    }

    throw std::runtime_error("Variable not found: " + name);
}

int SymbolTable::indexOf(const std::string& name) const {
    // Сначала проверяем подпрограмму
    auto it = subroutineTable.find(name);
    if (it != subroutineTable.end()) {
        return it->second.index;
    }

    // Затем проверяем класс
    it = classTable.find(name);
    if (it != classTable.end()) {
        return it->second.index;
    }

    throw std::runtime_error("Variable not found: " + name);
}