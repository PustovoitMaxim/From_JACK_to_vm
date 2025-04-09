#pragma once
#include <string>
#include <unordered_map>

enum class VarKind { STATIC, FIELD, ARG, VAR, NONE };

class SymbolTable {
public:
    SymbolTable();

    // ������ ����� ������������ (���������� ������� ARG � VAR)
    void startSubroutine();

    // �������� ���������� � �������
    void define(const std::string& name, const std::string& type, VarKind kind);

    // ���������� ���������� ��������� ����
    int varCount(VarKind kind) const;

    // �������� ���������� � ����������
    VarKind kindOf(const std::string& name) const;
    std::string typeOf(const std::string& name) const;
    int indexOf(const std::string& name) const;

private:
    struct Symbol {
        std::string type;
        VarKind kind;
        int index;
    };

    // ������� ��������
    std::unordered_map<std::string, Symbol> classTable;      // STATIC, FIELD
    std::unordered_map<std::string, Symbol> subroutineTable; // ARG, VAR

    // �������� ����������
    int staticCount;
    int fieldCount;
    int argCount;
    int varCount_;
};