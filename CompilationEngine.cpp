#include "CompilationEngine.h"
#include <sstream>
#include <stdexcept>
#include <iostream>

// Конструктор
CompilationEngine::CompilationEngine(JackTokenizer& t,
    VMWriter& v,
    SymbolTable& s,
    const std::string& cName)
    : tokenizer(t), vmWriter(v), symbolTable(s), className(cName) {
    tokenizer.advance(); // Загружаем первый токен
}

// Компиляция класса
void CompilationEngine::compileClass() {
    consumeKeyword(Keyword::CLASS);
    className = tokenizer.identifier();
    eat();
    consumeSymbol("{");

    while (tokenizer.tokenType() == TokenType::KEYWORD &&
        (tokenizer.keyWord() == Keyword::STATIC ||
            tokenizer.keyWord() == Keyword::FIELD)) {
        compileClassVarDec();
    }

    while (tokenizer.tokenType() == TokenType::KEYWORD &&
        (tokenizer.keyWord() == Keyword::CONSTRUCTOR ||
            tokenizer.keyWord() == Keyword::FUNCTION ||
            tokenizer.keyWord() == Keyword::METHOD)) {
        compileSubroutine();
    }

    consumeSymbol("}");
}

// Компиляция переменных класса
void CompilationEngine::compileClassVarDec() {
    VarKind kind;
    switch (tokenizer.keyWord()) {
    case Keyword::STATIC: kind = VarKind::STATIC; break;
    case Keyword::FIELD:  kind = VarKind::FIELD; break;
    default: throw std::runtime_error("Invalid class variable");
    }
    eat();

    std::string type = tokenizer.identifier();
    eat();

    do {
        std::string name = tokenizer.identifier();
        symbolTable.define(name, type, kind);
        eat();
    } while (tokenizer.symbol() == ",");

    consumeSymbol(";");
}

// Компиляция метода/функции
void CompilationEngine::compileSubroutine() {
    symbolTable.startSubroutine();

    Keyword subroutineType = tokenizer.keyWord();
    eat();

    // Определяем возвращаемый тип метода
    std::string returnType;
    if (tokenizer.tokenType() == TokenType::KEYWORD && tokenizer.keyWord() == Keyword::VOID) {
        returnType = "void";
        eat();
    }
    else {
        // Обработка int, char, boolean или пользовательского типа
        if (tokenizer.tokenType() == TokenType::KEYWORD) {
            returnType = keywordToString(tokenizer.keyWord());
        }
        else {
            returnType = tokenizer.identifier();
        }
        eat();
    }

    // Сохраняем возвращаемый тип в таблице символов
    std::string subroutineName = tokenizer.identifier();
    symbolTable.defineMethod(className + "." + subroutineName, returnType);

    // Формируем полное имя подпрограммы
    currentSubroutine = className + "." + subroutineName;
    eat();

    consumeSymbol("(");
    compileParameterList();
    consumeSymbol(")");

    // Тело подпрограммы
    consumeSymbol("{");
    while (tokenizer.tokenType() == TokenType::KEYWORD && tokenizer.keyWord() == Keyword::VAR) {
        compileVarDec();
    }

    // Генерация объявления функции
    vmWriter.writeFunction(currentSubroutine, symbolTable.varCount(VarKind::VAR));

    // Обработка this
    switch (subroutineType) {
    case Keyword::CONSTRUCTOR: {
        int fieldCount = symbolTable.varCount(VarKind::FIELD);
        vmWriter.writePush("constant", fieldCount);
        vmWriter.writeCall("Memory.alloc", 1);
        vmWriter.writePop("pointer", 0);
        break;
    }
    case Keyword::METHOD: {
        symbolTable.define("this", className, VarKind::ARG);
        vmWriter.writePush("argument", 0);
        vmWriter.writePop("pointer", 0);
        break;
    }
    case Keyword::FUNCTION: {
        break;
    }
    default:
        throw std::runtime_error("Invalid subroutine type");
    }

    compileStatements();
    consumeSymbol("}");
}

// Компиляция оператора let
void CompilationEngine::compileLet() {
    consumeKeyword(Keyword::LET);
    std::string varName = tokenizer.identifier();
    eat();

    bool isArray = false;
    VarKind kind;
    int varIndex;

    // Обработка индекса массива (arr[i])
    if (tokenizer.symbol() == "[") {
        isArray = true;
        eat();

        // Получаем информацию о переменной
        kind = symbolTable.kindOf(varName);
        varIndex = symbolTable.indexOf(varName);

        // Генерируем код для вычисления адреса элемента
        // 1. Загружаем базовый адрес массива
        vmWriter.writePush(kindToSegment(kind), varIndex);

        // 2. Компилируем индекс (выражение внутри [])
        compileExpression();
        consumeSymbol("]");

        // 3. Вычисляем адрес элемента: base + index
        vmWriter.writeArithmetic("add");

        // 4. Сохраняем адрес в регистре 'that'
        vmWriter.writePop("pointer", 1);
    }

    // Обработка правой части присваивания
    consumeSymbol("=");
    compileExpression();
    consumeSymbol(";");

    // Запись значения в память
    if (isArray) {
        // Записываем значение в вычисленный адрес (that[0])
        vmWriter.writePop("that", 0);
    }
    else {
        // Запись в обычную переменную
        kind = symbolTable.kindOf(varName);
        varIndex = symbolTable.indexOf(varName);
        vmWriter.writePop(kindToSegment(kind), varIndex);
    }
}
// Компиляция условия if
void CompilationEngine::compileIf() {
    std::string elseLabel = generateLabel("ELSE");
    std::string endLabel = generateLabel("END_IF");

    consumeKeyword(Keyword::IF);
    consumeSymbol("(");
    compileExpression();
    consumeSymbol(")");

    // Условие: если выражение ложно, переходим к else
    vmWriter.writeIf(elseLabel);
    // Пропускаем блок else
    vmWriter.writeGoto(endLabel);
    // Метка else
    vmWriter.writeLabel(elseLabel);
    // Блок if (выполняется, если условие истинно)
    consumeSymbol("{");
    compileStatements();
    consumeSymbol("}");

    // Блок else (если есть)
    if (tokenizer.tokenType() == TokenType::KEYWORD && tokenizer.keyWord() == Keyword::ELSE) {
        eat();
        consumeSymbol("{");
        compileStatements();
        consumeSymbol("}");
    }

    // Метка конца условия
    vmWriter.writeLabel(endLabel);
}
std::string CompilationEngine::kindToSegment(VarKind kind) const {
    switch (kind) {
    case VarKind::STATIC: return "static";
    case VarKind::FIELD:  return "this";
    case VarKind::ARG:    return "argument";
    case VarKind::VAR:    return "local";
    default:
        throw std::runtime_error("Invalid variable kind");
    }
}
void CompilationEngine::eat() {
    if (tokenizer.hasMoreTokens()) {
        tokenizer.advance();
    }
}
void CompilationEngine::compileParameterList() {
    if (tokenizer.tokenType() == TokenType::SYMBOL && tokenizer.symbol() == ")") {
        return; // Пустой список параметров
    }

    while (true) {
        // 1. Обработка типа параметра
        std::string type;
        if (tokenizer.tokenType() == TokenType::KEYWORD) {
            // Проверяем допустимые базовые типы
            if (tokenizer.keyWord() == Keyword::INT ||
                tokenizer.keyWord() == Keyword::CHAR ||
                tokenizer.keyWord() == Keyword::BOOLEAN) {
                type = keywordToString(tokenizer.keyWord());
            }
            else {
                throw std::runtime_error("Invalid parameter type");
            }
        }
        else if (tokenizer.tokenType() == TokenType::IDENTIFIER) {
            type = tokenizer.identifier(); // Пользовательский тип
        }
        else {
            throw std::runtime_error("Expected parameter type");
        }
        eat();

        // 2. Обработка имени параметра
        if (tokenizer.tokenType() != TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected parameter name");
        }
        std::string name = tokenizer.identifier();
        eat();

        // 3. Добавление в таблицу символов
        symbolTable.define(name, type, VarKind::ARG);

        // 4. Проверка следующего параметра
        if (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != ",") {
            break;
        }
        eat(); // Пропускаем запятую
    }
}

void CompilationEngine::compileVarDec() {
    consumeKeyword(Keyword::VAR);
    std::string type = tokenizer.identifier();
    eat();
    
    do {
        std::string name = tokenizer.identifier();
        symbolTable.define(name, type, VarKind::VAR);
        eat();
    } while (tokenizer.symbol() == ",");
    
    consumeSymbol(";");
}

void CompilationEngine::compileStatements() {
    while (tokenizer.tokenType() == TokenType::KEYWORD) {
        switch (tokenizer.keyWord()) {
            case Keyword::LET: compileLet(); break;
            case Keyword::IF: compileIf(); break;
            case Keyword::WHILE: compileWhile(); break;
            case Keyword::DO: compileDo(); break;
            case Keyword::RETURN: compileReturn(); break;
            default: return;
        }
    }
}

void CompilationEngine::compileWhile() {
    std::string labelStart = generateLabel("WHILE_START");
    std::string labelEnd = generateLabel("WHILE_END");
    
    consumeKeyword(Keyword::WHILE);
    consumeSymbol("(");
    
    vmWriter.writeLabel(labelStart);
    compileExpression();
    vmWriter.writeArithmetic("not");
    vmWriter.writeIf(labelEnd);
    
    consumeSymbol(")");
    consumeSymbol("{");
    compileStatements();
    consumeSymbol("}");
    
    vmWriter.writeGoto(labelStart);
    vmWriter.writeLabel(labelEnd);
}

void CompilationEngine::compileDo() {
    consumeKeyword(Keyword::DO);

    std::string identifier = tokenizer.identifier();
    eat();

    std::string fullMethodName;
    int nArgs = 0;
    bool isBuiltIn = false;

    if (tokenizer.symbol() == "(") {
        // Вызов метода текущего класса (например, Main.doSomething())
        eat();
        compileExpressionList();
        nArgs = currentExpressionCount;
        fullMethodName = className + "." + identifier;
        isBuiltIn = isBuiltInClass(className);
        vmWriter.writeCall(fullMethodName, nArgs);
        consumeSymbol(")");
    }
    else if (tokenizer.symbol() == ".") {
        eat();
        std::string methodName = tokenizer.identifier();
        eat();
        consumeSymbol("(");
        compileExpressionList();
        nArgs = currentExpressionCount;
        consumeSymbol(")");

        if (symbolTable.kindOf(identifier) != VarKind::NONE) {
            // Вызов метода объекта
            VarKind kind = symbolTable.kindOf(identifier);
            std::string className = symbolTable.typeOf(identifier);
            fullMethodName = className + "." + methodName;
            isBuiltIn = isBuiltInClass(className);
            vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(identifier));
            vmWriter.writeCall(fullMethodName, nArgs + 1);
        }
        else {
            // Статический вызов (например, Math.multiply())
            fullMethodName = identifier + "." + methodName;
            isBuiltIn = isBuiltInClass(identifier);
            vmWriter.writeCall(fullMethodName, nArgs);
        }
    }

    consumeSymbol(";");

    // Удаляем результат только для non-void методов пользовательских классов
    std::string returnType = symbolTable.getMethodReturnType(fullMethodName);
    if (returnType != "void") {
            vmWriter.writePop("temp", 0);
        }
}
void CompilationEngine::compileReturn() {
    consumeKeyword(Keyword::RETURN);
    
    if (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != ";") {
        compileExpression();
    } else {
        // Для void-функций
        vmWriter.writePush("constant", 0);
    }
    
    vmWriter.writeReturn();
    consumeSymbol(";");
}

void CompilationEngine::compileExpression() {
    compileTerm();
    
    while (isOperator(tokenizer.symbol())) {
        std::string op = tokenizer.symbol();
        eat();
        compileTerm();
        emitOperator(op);
    }
}

void CompilationEngine::compileTerm() {
    switch (tokenizer.tokenType()) {
        case TokenType::INT_CONST:
            vmWriter.writePush("constant", tokenizer.intVal());
            eat();
            break;
            
        case TokenType::STRING_CONST: {
            std::string str = tokenizer.stringVal();
            vmWriter.writePush("constant", str.length());
            vmWriter.writeCall("String.new", 1);
            for (char c : str) {
                vmWriter.writePush("constant", c);
                vmWriter.writeCall("String.appendChar", 2);
            }
            eat();
            break;
        }
            
        case TokenType::KEYWORD:
            switch (tokenizer.keyWord()) {
                case Keyword::TRUE:
                    vmWriter.writePush("constant", 1);
                    vmWriter.writeArithmetic("neg");
                    break;
                case Keyword::FALSE:
                case Keyword::NULL_:
                    vmWriter.writePush("constant", 0);
                    break;
                case Keyword::THIS:
                    vmWriter.writePush("pointer", 0);
                    break;
                default:
                    throw std::runtime_error("Invalid keyword constant");
            }
            eat();
            break;
            
        case TokenType::IDENTIFIER: {
            std::string identifier = tokenizer.identifier();
            eat();
            
            if (tokenizer.symbol() == "[") {
                // Обработка массива
                eat();
                compileExpression();
                consumeSymbol("]");
                
                VarKind kind = symbolTable.kindOf(identifier);
                vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(identifier));
                vmWriter.writeArithmetic("add");
                vmWriter.writePop("pointer", 1);
                vmWriter.writePush("that", 0);
            } 
            else if (tokenizer.symbol() == "(" || tokenizer.symbol() == ".") {
                // Обработка вызовов методов
                compileSubroutineCall(identifier);
            }
            else {
                // Обычная переменная
                VarKind kind = symbolTable.kindOf(identifier);
                vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(identifier));
            }
            break;
        }
            
        case TokenType::SYMBOL:
            if (tokenizer.symbol() == "(") {
                eat();
                compileExpression();
                consumeSymbol(")");
            } 
            else if (isUnaryOp()) {
                std::string op = tokenizer.symbol();
                eat();
                compileTerm();
                if (op == "-") vmWriter.writeArithmetic("neg");
                else if (op == "~") vmWriter.writeArithmetic("not");
            }
            break;
            
        default:
            throw std::runtime_error("Unexpected token type");
    }
}

void CompilationEngine::compileExpressionList() {
    currentExpressionCount = 0; // Сброс счетчика
    while (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != ")") {
        compileExpression();
        currentExpressionCount++; // Увеличиваем счетчик
        if (tokenizer.symbol() == ",") {
            eat();
        }
        else {
            break;
        }
    }
}
// Проверяет, что текущий токен соответствует ожидаемому значению (для идентификаторов/ключевых слов)
void CompilationEngine::expect(const std::string& expected) {
    if (tokenizer.tokenType() == TokenType::IDENTIFIER) {
        if (tokenizer.identifier() != expected) {
            throw std::runtime_error("Expected identifier '" + expected + "'");
        }
    }
    else if (tokenizer.tokenType() == TokenType::KEYWORD) {
        if (keywordToString(tokenizer.keyWord()) != expected) {
            throw std::runtime_error("Expected keyword '" + expected + "'");
        }
    }
    else {
        throw std::runtime_error("Unexpected token type");
    }
}

// Потребляет символ, проверяя его корректность
void CompilationEngine::consumeSymbol(std::string symbol) {
    if (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != symbol) {
        std::string msg = "Expected '" + symbol + "' got '";
        msg += (tokenizer.tokenType() == TokenType::SYMBOL)
            ? tokenizer.symbol()
            : "non-symbol";
        msg += "'";
        throw std::runtime_error(msg);
    }
    eat(); // Переходим к следующему токену
}

// Потребляет ключевое слово, проверяя его корректность
void CompilationEngine::consumeKeyword(Keyword expectedKeyword) {
    // Получаем текстовое представление текущего токена
    std::string currentTokenStr;

    switch (tokenizer.tokenType()) {
    case TokenType::KEYWORD:
        currentTokenStr = keywordToString(tokenizer.keyWord());
        break;
    case TokenType::IDENTIFIER:
        currentTokenStr = tokenizer.identifier();
        break;
    case TokenType::SYMBOL:
        currentTokenStr = tokenizer.symbol();
        break;
    case TokenType::INT_CONST:
        currentTokenStr = std::to_string(tokenizer.intVal());
        break;
    case TokenType::STRING_CONST:
        currentTokenStr = "\"" + tokenizer.stringVal() + "\"";
        break;
    default:
        currentTokenStr = "UNKNOWN";
    }

    std::cout << "DEBUG: Current token: " << currentTokenStr << std::endl;

    if (tokenizer.tokenType() != TokenType::KEYWORD || tokenizer.keyWord() != expectedKeyword) {
        std::string expected = keywordToString(expectedKeyword);
        std::string actual = (tokenizer.tokenType() == TokenType::KEYWORD)
            ? keywordToString(tokenizer.keyWord())
            : "non-keyword";
        throw std::runtime_error("Expected keyword '" + expected + "' got '" + actual + "'");
    }
    eat();
}

// Генерирует уникальные метки для управления потоком
std::string CompilationEngine::generateLabel(const std::string& prefix) {
    return className + "_" + prefix + "_" + std::to_string(labelCounter++);
}

// Вспомогательная функция для преобразования Keyword в строку (должна быть реализована отдельно)

    bool CompilationEngine::isOperator(std::string c) const {
        static const std::unordered_set<std::string> operators = {
            "+", "-", "*", "/",
            "&", "|", "<", ">", "=","<=",">="
        };
        return operators.count(c) != 0;
    }
    // Определяет, является ли текущий символ унарным оператором
    bool CompilationEngine::isUnaryOp() const {
        if (tokenizer.tokenType() != TokenType::SYMBOL) return false;
        std::string c = tokenizer.symbol();
        return c == "-" || c == "~";
    }
    // Преобразует операторы Jack в VM-команды
    void CompilationEngine::emitOperator(const std::string& op) {
        // Обработка составных операторов сравнения
        if (op == "<=") {
            vmWriter.writeArithmetic("gt");
            vmWriter.writeArithmetic("not");
            return;
        }
        if (op == ">=") {
            vmWriter.writeArithmetic("lt");
            vmWriter.writeArithmetic("not");
            return;
        }

        // Таблица соответствия операторов и VM-команд
        static const std::unordered_map<std::string, std::string> opMap = {
            {"+", "add"},
            {"-", "sub"},
            {"*", "call Math.multiply 2"},
            {"/", "call Math.divide 2"},
            {"&", "and"},
            {"|", "or"},
            {"<", "lt"},
            {">", "gt"},
            {"=", "eq"},
            {"~", "not"}
        };

        auto it = opMap.find(op);
        if (it != opMap.end()) {
            const std::string& cmd = it->second;

            if (cmd.starts_with("call")) {
                // Исправление: корректное извлечение функции и аргументов
                size_t firstSpace = cmd.find(' ');
                size_t secondSpace = cmd.find(' ', firstSpace + 1);

                if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
                    throw std::runtime_error("Invalid call format: " + cmd);
                }

                std::string func = cmd.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                int nArgs = std::stoi(cmd.substr(secondSpace + 1));

                vmWriter.writeCall(func, nArgs);
            }
            else {
                vmWriter.writeArithmetic(cmd);
            }
        }
        else {
            throw std::runtime_error("Unknown operator: " + op);
        }
    }
    void CompilationEngine::compileSubroutineCall(const std::string& identifier) {
        std::string classOrVarName = identifier;
        std::string subroutineName;
        int nArgs = 0;
        bool isMethodCall = false;
        bool isBuiltIn = false;
        std::string fullName;

        // Обработка вызовов вида object.method()
        if (tokenizer.symbol() == ".") {
            eat(); // Съедаем точку

            subroutineName = tokenizer.identifier();
            eat();

            VarKind kind = symbolTable.kindOf(classOrVarName);
            if (kind != VarKind::NONE) {
                // 1. Загружаем объект (this) в стек ПЕРВЫМ
                vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(classOrVarName));
                classOrVarName = symbolTable.typeOf(classOrVarName);
                isMethodCall = true;
                nArgs = 1; // Учитываем this
            }
            else {
                // Статический вызов (например, String.new)
                isMethodCall = false;
            }

            fullName = classOrVarName + "." + subroutineName;
            isBuiltIn = isBuiltInClass(classOrVarName);
        }
        else {
            // Обработка вызовов вида method() (неявный this)
            isMethodCall = true;
            subroutineName = classOrVarName;
            fullName = className + "." + subroutineName;
            isBuiltIn = isBuiltInClass(className);

            // Загружаем неявный this
            vmWriter.writePush("pointer", 0);
            nArgs = 1;
        }

        // Обработка аргументов (после this)
        consumeSymbol("(");
        compileExpressionList(); // Аргументы добавляются в стек после this
        nArgs += currentExpressionCount;
        consumeSymbol(")");

        // Вызов метода
        vmWriter.writeCall(fullName, nArgs);

        // Всегда удаляем результат для оператора do
        vmWriter.writePop("temp", 0);
    }
    bool CompilationEngine::isBuiltInClass(const std::string& className) const {
        const std::unordered_set<std::string> builtInClasses = {
            "Array", "Math", "Memory", "Screen",
            "String", "Keyboard", "Sys", "Output"
        };
        return builtInClasses.count(className) != 0;
    }