#include <iostream>
#include <filesystem>
#include <vector>
#include "JackTokenizer.h"
#include "CompilationEngine.h"

namespace fs = std::filesystem;

// Получить список .jack файлов для обработки
std::vector<fs::path> getJackFiles(const fs::path& path) {
    std::vector<fs::path> files;

    if (fs::is_directory(path)) {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".jack") {
                files.push_back(entry.path());
            }
        }
    }
    else if (path.extension() == ".jack") {
        files.push_back(path);
    }

    return files;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.jack|directory>\n";
        return 1;
    }

    const fs::path inputPath(argv[1]);

    try {
        // Проверка существования пути
        if (!fs::exists(inputPath)) {
            throw std::runtime_error("Path does not exist: " + inputPath.string());
        }

        // Получаем список файлов для обработки
        auto jackFiles = getJackFiles(inputPath);
        if (jackFiles.empty()) {
            throw std::runtime_error("No .jack files found");
        }

        // Обрабатываем каждый файл
        for (const auto& jackFile : jackFiles) {
            // Создаем выходной путь
            fs::path vmPath = jackFile;
            vmPath.replace_extension(".vm");

            // Инициализируем компоненты компилятора
            JackTokenizer tokenizer(jackFile.string());
            VMWriter vmWriter(vmPath.string());
            SymbolTable symbolTable;

            // Получаем имя класса из имени файла
            std::string className = jackFile.stem().string();

            // Компилируем
            CompilationEngine compiler(
                tokenizer,
                vmWriter,
                symbolTable,
                className
            );
            std::string cmd = "call  Math.divide  2";
            std::string funcName = cmd.substr(5, cmd.find(' ') - 5);
            std::string argsStr = cmd.substr(cmd.find_last_of(' ') + 1);
            
            compiler.compileClass();
            vmWriter.close();

            std::cout << "Compiled: "
                << jackFile.filename() << " -> "
                << vmPath.filename() << "\n";
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}