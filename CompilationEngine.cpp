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
    tokenizer.advance(); 
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

    std::string returnType;
    if (tokenizer.tokenType() == TokenType::KEYWORD && tokenizer.keyWord() == Keyword::VOID) {
        returnType = "void";
        eat();
    }
    else {
        
        if (tokenizer.tokenType() == TokenType::KEYWORD) {
            returnType = keywordToString(tokenizer.keyWord());
        }
        else {
            returnType = tokenizer.identifier();
        }
        eat();
    }

    
    std::string subroutineName = tokenizer.identifier();
    symbolTable.defineMethod(className + "." + subroutineName, returnType);

    
    currentSubroutine = className + "." + subroutineName;
    eat();

    consumeSymbol("(");
    compileParameterList();
    consumeSymbol(")");

    
    consumeSymbol("{");
    while (tokenizer.tokenType() == TokenType::KEYWORD && tokenizer.keyWord() == Keyword::VAR) {
        compileVarDec();
    }

    
    vmWriter.writeFunction(currentSubroutine, symbolTable.varCount(VarKind::VAR));

    
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

    
    if (tokenizer.symbol() == "[") {
        isArray = true;
        eat();

        
        kind = symbolTable.kindOf(varName);
        varIndex = symbolTable.indexOf(varName);

        
        vmWriter.writePush(kindToSegment(kind), varIndex);

        
        compileExpression();
        consumeSymbol("]");

        
        vmWriter.writeArithmetic("add");

        
        vmWriter.writePop("pointer", 1);
    }

    
    consumeSymbol("=");
    compileExpression();
    consumeSymbol(";");

    
    if (isArray) {
        
        vmWriter.writePop("that", 0);
    }
    else {
       
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

    
    vmWriter.writeIf(elseLabel);
    
    vmWriter.writeGoto(endLabel);
    
    vmWriter.writeLabel(elseLabel);
    
    consumeSymbol("{");
    compileStatements();
    consumeSymbol("}");

    if (tokenizer.tokenType() == TokenType::KEYWORD && tokenizer.keyWord() == Keyword::ELSE) {
        eat();
        consumeSymbol("{");
        compileStatements();
        consumeSymbol("}");
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
    if (tokenizer.tokenType() == TokenType::SYMBOL && tokenizer.symbol() == ")") {
        return; // Пустой список параметров
    }

    while (true) {
        std::string type;
        if (tokenizer.tokenType() == TokenType::KEYWORD) {
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
            type = tokenizer.identifier();
        }
        else {
            throw std::runtime_error("Expected parameter type");
        }
        eat();
        if (tokenizer.tokenType() != TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected parameter name");
        }
        std::string name = tokenizer.identifier();
        eat();

        symbolTable.define(name, type, VarKind::ARG);

        if (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != ",") {
            break;
        }
        eat();
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
            VarKind kind = symbolTable.kindOf(identifier);
            std::string className = symbolTable.typeOf(identifier);
            fullMethodName = className + "." + methodName;
            isBuiltIn = isBuiltInClass(className);
            vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(identifier));
            vmWriter.writeCall(fullMethodName, nArgs + 1);
        }
        else {
            fullMethodName = identifier + "." + methodName;
            isBuiltIn = isBuiltInClass(identifier);
            vmWriter.writeCall(fullMethodName, nArgs);
        }
    }

    consumeSymbol(";");

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
                compileSubroutineCall(identifier);
            }
            else {
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
    currentExpressionCount = 0; 
    while (tokenizer.tokenType() != TokenType::SYMBOL || tokenizer.symbol() != ")") {
        compileExpression();
        currentExpressionCount++; 
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
    eat(); 
}

// Потребляет ключевое слово, проверяя его корректность
void CompilationEngine::consumeKeyword(Keyword expectedKeyword) {
 
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

        if (tokenizer.symbol() == ".") {
            eat(); 

            subroutineName = tokenizer.identifier();
            eat();

            VarKind kind = symbolTable.kindOf(classOrVarName);
            if (kind != VarKind::NONE) {
             
                vmWriter.writePush(kindToSegment(kind), symbolTable.indexOf(classOrVarName));
                classOrVarName = symbolTable.typeOf(classOrVarName);
                isMethodCall = true;
                nArgs = 1; 
            }
            else {
                isMethodCall = false;
            }

            fullName = classOrVarName + "." + subroutineName;
            isBuiltIn = isBuiltInClass(classOrVarName);
        }
        else {
            isMethodCall = true;
            subroutineName = classOrVarName;
            fullName = className + "." + subroutineName;
            isBuiltIn = isBuiltInClass(className);

            vmWriter.writePush("pointer", 0);
            nArgs = 1;
        }

        consumeSymbol("(");
        compileExpressionList(); 
        nArgs += currentExpressionCount;
        consumeSymbol(")");

        vmWriter.writeCall(fullName, nArgs);

        vmWriter.writePop("temp", 0);
    }
    bool CompilationEngine::isBuiltInClass(const std::string& className) const {
        const std::unordered_set<std::string> builtInClasses = {
            "Array", "Math", "Memory", "Screen",
            "String", "Keyboard", "Sys", "Output"
        };
        return builtInClasses.count(className) != 0;
    }