#include "VMWriter.h"
#include <stdexcept>
#include <iostream>

VMWriter::VMWriter(const std::string& filename) {
    outputFile.open(filename);
    if (!outputFile.is_open()) {
        throw std::runtime_error("Failed to open output file: " + filename);
    }
    isFileOpen = true;
}

VMWriter::~VMWriter() {
    close();
}

void VMWriter::checkFile() const {
    if (!isFileOpen) {
        throw std::runtime_error("VMWriter: File is not open");
    }
}

void VMWriter::writePush(const std::string& segment, int index) {
    checkFile();
    outputFile << "push " << segment << " " << index << "\n";
}

void VMWriter::writePop(const std::string& segment, int index) {
    checkFile();
    outputFile << "pop " << segment << " " << index << "\n";
}

void VMWriter::writeArithmetic(const std::string& command) {
    checkFile();
    outputFile << command << "\n";
}

void VMWriter::writeLabel(const std::string& label) {
    checkFile();
    outputFile << "label " << label << "\n";
}

void VMWriter::writeGoto(const std::string& label) {
    checkFile();
    outputFile << "goto " << label << "\n";
}

void VMWriter::writeIf(const std::string& label) {
    checkFile();
    outputFile << "if-goto " << label << "\n";
}

void VMWriter::writeCall(const std::string& name, int nArgs) {
    checkFile();
    std::cout << "call " << name << " " << nArgs << "\n";
    outputFile << "call " << name << " " << nArgs << "\n";
}

void VMWriter::writeFunction(const std::string& name, int nLocals) {
    checkFile();
    outputFile << "function " << name << " " << nLocals << "\n";
}

void VMWriter::writeReturn() {
    checkFile();
    outputFile << "return\n";
}

void VMWriter::close() {
    if (isFileOpen) {
        outputFile.close();
        isFileOpen = false;
    }
}