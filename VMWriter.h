#pragma once
#include <fstream>
#include <string>

class VMWriter {
public:
    // �����������: ��������� �������� ���� .vm
    explicit VMWriter(const std::string& filename);

    // ����������: ��������� ���� ��� �������������
    ~VMWriter();

    // ���������� ������� push
    void writePush(const std::string& segment, int index);

    // ���������� ������� pop
    void writePop(const std::string& segment, int index);

    // ���������� ��������������/���������� �������
    void writeArithmetic(const std::string& command);

    // ���������� �����
    void writeLabel(const std::string& label);

    // ���������� ����������� ������� (goto)
    void writeGoto(const std::string& label);

    // ���������� �������� ������� (if-goto)
    void writeIf(const std::string& label);

    // ���������� ����� �������
    void writeCall(const std::string& name, int nArgs);

    // ���������� ���������� �������
    void writeFunction(const std::string& name, int nLocals);

    // ���������� return
    void writeReturn();

    // ��������� �������� ����
    void close();

private:
    std::ofstream outputFile;
    bool isFileOpen = false;

    void checkFile() const;
};