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
    consumeSymbol('{');

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

    consumeSymbol('}');
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
    } while (tokenizer.symbol() == ',');

    consumeSymbol(';');
}

// Компиляция метода/функции
void CompilationEngine::compileSubroutine() {
    symbolTable.startSubroutine();

    Keyword subroutineType = tokenizer.keyWord();
    eat();

    // Тип возврата
    if (tokenizer.tokenType() == TokenType::KEYWORD &&
        tokenizer.keyWord() == Keyword::VOID) {
        eat();
    }
    else {
        eat(); // тип
    }

    currentSubroutine = className + "." + tokenizer.identifier();
    eat();
    consumeSymbol('(');
    compileParameterList();
    consumeSymbol(')');

    // Тело подпрограммы
    consumeSymbol('{');
    while (tokenizer.tokenType() == TokenType::KEYWORD &&
        tokenizer.keyWord() == Keyword::VAR) {
        compileVarDec();
    }

    vmWriter.writeFunction(currentSubroutine, symbolTable.varCount(VarKind::VAR));

    if (subroutineType == Keyword::CONSTRUCTOR) {
        // Выделение памяти для объекта
        vmWriter.writePush("constant", symbolTable.varCount(VarKind::FIELD));
        vmWriter.writeCall("Memory.alloc", 1);
        vmWriter.writePop("pointer", 0);
    }

    compileStatements();
    consumeSymbol('}');
}

// Компиляция оператора let
void CompilationEngine::compileLet() {
    consumeKeyword(Keyword::LET);
    std::string varName = tokenizer.identifier();
    eat();

    bool isArray = false;
    if (tokenizer.symbol() == '[') {
        isArray = true;
        eat();
        compileExpression();
        consumeSymbol(']');

        // Вычисление адреса массива
        VarKind kind = symbolTable.kindOf(varName);
        vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(varName));
        vmWriter.writeArithmetic("add");
    }

    consumeSymbol('=');
    compileExpression();
    consumeSymbol(';');

    if (isArray) {
        vmWriter.writePop("temp", 0);
        vmWriter.writePop("pointer", 1);
        vmWriter.writePush("temp", 0);
        vmWriter.writePop("that", 0);
    }
    else {
        VarKind kind = symbolTable.kindOf(varName);
        vmWriter.writePop(kindToSegment(kind), symbolTable.indexOf(varName));
    }
}

// Компиляция условия if
void CompilationEngine::compileIf() {
    std::string elseLabel = generateLabel("ELSE");
    std::string endLabel = generateLabel("END_IF");

    consumeKeyword(Keyword::IF);
    consumeSymbol('(');
    compileExpression();
    consumeSymbol(')');

    vmWriter.writeArithmetic("not");
    vmWriter.writeIf(elseLabel);

    consumeSymbol('{');
    compileStatements();
    consumeSymbol('}');

    vmWriter.writeGoto(endLabel);
    vmWriter.writeLabel(elseLabel);

    if (tokenizer.tokenType() == TokenType::KEYWORD &&
        tokenizer.keyWord() == Keyword::ELSE) {
        eat();
        consumeSymbol('{');
        compileStatements();
        consumeSymbol('}');
    }

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
    if (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != ')') {
        do {
            std::string type = tokenizer.identifier();
            eat();
            std::string name = tokenizer.identifier();
            symbolTable.define(name, type, VarKind::ARG);
            eat();
        } while (tokenizer.tokenType() == TokenType::SYMBOL && tokenizer.symbol() == ',');
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
    } while (tokenizer.symbol() == ',');
    
    consumeSymbol(';');
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
    consumeSymbol('(');
    
    vmWriter.writeLabel(labelStart);
    compileExpression();
    vmWriter.writeArithmetic("not");
    vmWriter.writeIf(labelEnd);
    
    consumeSymbol(')');
    consumeSymbol('{');
    compileStatements();
    consumeSymbol('}');
    
    vmWriter.writeGoto(labelStart);
    vmWriter.writeLabel(labelEnd);
}

void CompilationEngine::compileDo() {
    consumeKeyword(Keyword::DO);

    // Обработка вызовов вида ClassName.method() или method()
    std::string identifier = tokenizer.identifier();
    eat();

    if (tokenizer.symbol() == '(') {
        // Локальный вызов метода (использует this)
        eat();
        compileExpressionList(); // Теперь void
        int nArgs = currentExpressionCount; // Получаем количество из переменной класса
        vmWriter.writePush("pointer", 0); // this как первый аргумент
        vmWriter.writeCall(className + "." + identifier, nArgs + 1);
        consumeSymbol(')');
    }
    else if (tokenizer.symbol() == '.') {
        eat();
        std::string methodName = tokenizer.identifier();
        eat();
        consumeSymbol('(');
        compileExpressionList(); // Теперь void
        int nArgs = currentExpressionCount; // Получаем количество из переменной класса
        consumeSymbol(')');

        // Проверяем, объект ли это или класс
        if (symbolTable.kindOf(identifier) != VarKind::NONE) {
            // Вызов метода объекта
            VarKind kind = symbolTable.kindOf(identifier);
            vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(identifier));
            vmWriter.writeCall(symbolTable.typeOf(identifier) + "." + methodName, nArgs + 1);
        }
        else {
            // Статический вызов
            vmWriter.writeCall(identifier + "." + methodName, nArgs);
        }
    }

    consumeSymbol(';');
    vmWriter.writePop("temp", 0); // Игнорируем возвращаемое значение
}

void CompilationEngine::compileReturn() {
    consumeKeyword(Keyword::RETURN);
    
    if (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != ';') {
        compileExpression();
    } else {
        // Для void-функций
        vmWriter.writePush("constant", 0);
    }
    
    vmWriter.writeReturn();
    consumeSymbol(';');
}

void CompilationEngine::compileExpression() {
    compileTerm();
    
    while (isOperator(tokenizer.symbol())) {
        std::string op = std::string(1, tokenizer.symbol());
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
            
            if (tokenizer.symbol() == '[') {
                // Обработка массива
                eat();
                compileExpression();
                consumeSymbol(']');
                
                VarKind kind = symbolTable.kindOf(identifier);
                vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(identifier));
                vmWriter.writeArithmetic("add");
                vmWriter.writePop("pointer", 1);
                vmWriter.writePush("that", 0);
            } 
            else if (tokenizer.symbol() == '(' || tokenizer.symbol() == '.') {
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
            if (tokenizer.symbol() == '(') {
                eat();
                compileExpression();
                consumeSymbol(')');
            } 
            else if (isUnaryOp()) {
                char op = tokenizer.symbol();
                eat();
                compileTerm();
                if (op == '-') vmWriter.writeArithmetic("neg");
                else if (op == '~') vmWriter.writeArithmetic("not");
            }
            break;
            
        default:
            throw std::runtime_error("Unexpected token type");
    }
}

void CompilationEngine::compileExpressionList() {
    currentExpressionCount = 0; // Сброс счетчика
    while (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != ')') {
        compileExpression();
        currentExpressionCount++; // Увеличиваем счетчик
        if (tokenizer.symbol() == ',') {
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
void CompilationEngine::consumeSymbol(char symbol) {
    if (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != symbol) {
        std::string msg = "Expected '" + std::string(1, symbol) + "' got '";
        msg += (tokenizer.tokenType() == TokenType::SYMBOL)
            ? std::string(1, tokenizer.symbol())
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
        currentTokenStr = std::string(1, tokenizer.symbol());
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

    bool CompilationEngine::isOperator(char c) const {
        static const std::unordered_set<char> operators = {
            '+', '-', '*', '/',
            '&', '|', '<', '>', '='
        };
        return operators.count(c) != 0;
    }
    // Определяет, является ли текущий символ унарным оператором
    bool CompilationEngine::isUnaryOp() const {
        if (tokenizer.tokenType() != TokenType::SYMBOL) return false;
        char c = tokenizer.symbol();
        return c == '-' || c == '~';
    }
    // Преобразует операторы Jack в VM-команды
    void CompilationEngine::emitOperator(const std::string& op) {
        static const std::unordered_map<std::string, std::string> opMap = {
            {"+", "add"}, {"-", "sub"}, {"*", "call Math.multiply 2"},
            {"/", "call Math.divide 2"}, {"&", "and"}, {"|", "or"},
            {"<", "lt"}, {">", "gt"}, {"=", "eq"}, {"~", "not"}
        };

        // Проверка существования оператора
        if (!opMap.contains(op)) {
            throw std::runtime_error("Unknown operator: " + op);
        }

        const std::string& cmd = opMap.at(op);

        // Обработка арифметических команд
        if (cmd.starts_with("call")) {
            size_t first_space = cmd.find(' ');
            size_t last_space = cmd.find_last_of(' ');
            std::cout << "DEBUG: cmd='" << cmd
                << "', first_space=" << first_space
                << ", last_space=" << last_space
                << std::endl;
            // Валидация формата команды
            if (first_space == std::string::npos ||
                last_space == std::string::npos ||
                first_space >= last_space)
            {
                throw std::runtime_error("Invalid call format for operator " + op);
            }

            // Извлечение имени функции и аргументов
            std::string func = cmd.substr(5, last_space - 5);
            std::string args_str = cmd.substr(last_space + 1);
            try {
                int n_args = std::stoi(args_str);
                vmWriter.writeCall(func, n_args);
            }
            catch (const std::exception& e) {
                throw std::runtime_error("Invalid argument count for " + func + ": " + args_str);
            }
        }
        // Обработка обычных арифметических команд
        else {
            vmWriter.writeArithmetic(cmd);
        }
    }
    void CompilationEngine::compileSubroutineCall(const std::string& identifier) {
        std::string classOrVarName = identifier;
        std::string subroutineName;
        int nArgs = 0;
        bool isMethodCall = false;

        // Обработка разных форм вызова
        if (tokenizer.symbol() == '.') {
            eat(); // Пропускаем '.'
            subroutineName = tokenizer.identifier();
            eat();

            // Проверяем, является ли идентификатор объектом/классом
            VarKind kind = symbolTable.kindOf(classOrVarName);
            if (kind != VarKind::NONE) {
                // Случай 3: вызов метода объекта
                isMethodCall = true;
                vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(classOrVarName));
                classOrVarName = symbolTable.typeOf(classOrVarName);
                nArgs = 1; // this как первый аргумент
            }
        }
        else {
            // Случай 1: вызов метода текущего класса
            isMethodCall = true;
            subroutineName = classOrVarName;
            classOrVarName = className;
            vmWriter.writePush("pointer", 0); // this
            nArgs = 1;
        }

        // Обработка списка аргументов
        consumeSymbol('(');
        compileExpressionList(); // Обновляем currentExpressionCount
        nArgs += currentExpressionCount; // Используем значение из класса
        consumeSymbol(')');

        // Генерация команды вызова
        std::string fullName = classOrVarName + "." + subroutineName;
        vmWriter.writeCall(fullName, nArgs);

        // Для void-методов: очищаем возвращаемое значение
        if (isMethodCall) {
            vmWriter.writePop("temp", 0);
        }
    }