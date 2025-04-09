#pragma once
#include <fstream>
#include <string>

class VMWriter {
public:
    // Конструктор: открывает выходной файл .vm
    explicit VMWriter(const std::string& filename);

    // Деструктор: закрывает файл при необходимости
    ~VMWriter();

    // Записывает команду push
    void writePush(const std::string& segment, int index);

    // Записывает команду pop
    void writePop(const std::string& segment, int index);

    // Записывает арифметическую/логическую команду
    void writeArithmetic(const std::string& command);

    // Записывает метку
    void writeLabel(const std::string& label);

    // Записывает безусловный переход (goto)
    void writeGoto(const std::string& label);

    // Записывает условный переход (if-goto)
    void writeIf(const std::string& label);

    // Записывает вызов функции
    void writeCall(const std::string& name, int nArgs);

    // Записывает объявление функции
    void writeFunction(const std::string& name, int nLocals);

    // Записывает return
    void writeReturn();

    // Закрывает выходной файл
    void close();

private:
    std::ofstream outputFile;
    bool isFileOpen = false;

    void checkFile() const;
};