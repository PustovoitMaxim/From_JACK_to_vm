#include "JackTokenizer.h"
#include <cctype>
#include <stdexcept>
#include <iostream>
#include <algorithm>
// ������� ����� Jack
const std::unordered_set<char> JackTokenizer::symbols = {
    '{', '}', '(', ')', '[', ']', '.', ',', ';', '+',
    '-', '*', '/', '&', '|', '<', '>', '=', '~'
};

// ������������ ����� �������� ������
const std::unordered_map<std::string, Keyword> JackTokenizer::keywordMap = {
    {"class", Keyword::CLASS}, {"method", Keyword::METHOD},
    {"function", Keyword::FUNCTION}, {"constructor", Keyword::CONSTRUCTOR},
    {"int", Keyword::INT}, {"boolean", Keyword::BOOLEAN},
    {"char", Keyword::CHAR}, {"void", Keyword::VOID},
    {"var", Keyword::VAR}, {"static", Keyword::STATIC},
    {"field", Keyword::FIELD}, {"let", Keyword::LET},
    {"do", Keyword::DO}, {"if", Keyword::IF}, {"else", Keyword::ELSE},
    {"while", Keyword::WHILE}, {"return", Keyword::RETURN},
    {"true", Keyword::TRUE}, {"false", Keyword::FALSE},
    {"null", Keyword::NULL_}, {"this", Keyword::THIS}
};

JackTokenizer::JackTokenizer(const std::string& filename)
    : input(filename), currentType(TokenType::UNKNOWN) {
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
}

JackTokenizer::~JackTokenizer() {
    if (input.is_open()) input.close();
}

bool JackTokenizer::hasMoreTokens() const {
    return !input.eof();
}

void JackTokenizer::setDebugMode(bool mode) {
    debugMode = mode;
}
void JackTokenizer::readString() {
    input.get(); // ���������� ����������� ������� "
    currentString.clear();
    currentType = TokenType::STRING_CONST;

    while (!input.eof()) {
        char c = input.get();
        // ��������� �������� ����� ������ ������
        if (c == '\n') {
            throw std::runtime_error("Newline in string literal at line " + std::to_string(lineNumber));
        }
        // ����������� �������
        if (c == '"') {
            return;
        }
        currentString += c;
    }
    // ���� ����� �� ����� ����� ��� ����������� �������
    throw std::runtime_error("Unclosed string literal starting at line " + std::to_string(lineNumber));
}

void JackTokenizer::readNumber() {
    currentInt = 0;
    currentType = TokenType::INT_CONST;

    while (!input.eof() && isdigit(input.peek())) {
        currentInt = currentInt * 10 + (input.get() - '0');
    }
}

void JackTokenizer::readKeywordOrIdentifier() {
    std::string token;
    currentType = TokenType::IDENTIFIER;

    // ��������� �������, ���� ��� �������� ����������� ��� ��������������
    while (!input.eof() && (isalnum(input.peek()) || input.peek() == '_')) {
        token += input.get();
    }

    // ����������� � ������ ������� �������
    std::string lowerToken;
    for (char c : token) {
        // ���������� �������������� ��� tolower()
        lowerToken.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    // ���������, �������� �� ����� �������� ������
    if (keywordMap.find(lowerToken) != keywordMap.end()) {
        currentType = TokenType::KEYWORD;
        currentKeyword = keywordMap.at(lowerToken);
    }
    else {
        currentIdentifier = token;
    }
}
void JackTokenizer::readSymbol() {
    currentSymbol = input.get();
    currentType = TokenType::SYMBOL;

    // �������� �� ������������ �������
    static const std::unordered_set<char> validSymbols = {
        '{', '}', '(', ')', '[', ']', '.', ',', ';', '+',
        '-', '*', '/', '&', '|', '<', '>', '=', '~'
    };

    if (!validSymbols.count(currentSymbol)) {
        throw std::runtime_error("Invalid symbol: " + std::string(1, currentSymbol));
    }
}
void JackTokenizer::advance() {
    skipCommentsAndWhitespace();
    if (input.eof()) {
        currentType = TokenType::UNKNOWN;
        return;
    }
    // ��������� ���������� ����� ������ ��� ������� �����������
    size_t tokenLine = lineNumber;

    char c = input.peek();

    if (c == '"') {
        readString();
    }
    else if (isdigit(c)) {
        readNumber();
    }
    else if (isalpha(c) || c == '_') {
        readKeywordOrIdentifier();
    }
    else {
        readSymbol();
    }

    // ����� ���������� ����������
    if (debugMode) {
        getCurrentTokenInfo();
        std::cout <<" (processed at line " << tokenLine << ")"
            << std::endl;
    }
}

void JackTokenizer::skipCommentsAndWhitespace() {
    bool inComment = false;

    while (!input.eof()) {
        char c = input.peek();

        // ��������� �������� ������
        if (c == '\n') {
            lineNumber++;
            input.get();
            continue;
        }

        // ���������� ������� � ����
        if (isspace(c)) {
            input.get();
            continue;
        }

        // ��������� ������ �����������
        if (!inComment && c == '/') {
            input.get(); // ������� '/'
            char next = input.peek();

            // ������������ �����������
            if (next == '/') {
                input.get(); // ������� ������ '/'
                // ���������� �� ����� ������
                while (input.peek() != '\n' && !input.eof()) input.get();
                continue;
            }

            // ������������� �����������
            else if (next == '*') {
                input.get(); // ������� '*'
                inComment = true;
                bool prevStar = false;

                while (!input.eof()) {
                    char commentChar = input.get();

                    // ��������� �������� ����� � ������������
                    if (commentChar == '\n') lineNumber++;

                    // ��������� ��������� �����������
                    if (prevStar && commentChar == '/') {
                        inComment = false;
                        break;
                    }
                    prevStar = (commentChar == '*');
                }
                continue;
            }

            // �� ����������� - ���������� '/' ������� � �����
            else {
                input.putback('/');
                break;
            }
        }

        // ������� �� �����, ���� �� ����������� � �� ������
        if (!inComment && !isspace(c)) break;

        input.get(); // ���������� ���������
    }
}
void JackTokenizer::readNextToken() {
    char c = input.peek();

    // ��������� ���������
    if (c == '"') {
        input.get();
        currentType = TokenType::STRING_CONST;
        while (input.get(c) && c != '"') {
            currentString += c;
        }
        return;
    }

    // �������� ���������
    if (isdigit(c)) {
        currentType = TokenType::INT_CONST;
        std::string num;
        while (isdigit(input.peek())) {
            num += input.get();
        }
        currentInt = std::stoi(num);
        return;
    }

    // ������
    if (symbols.count(c)) {
        currentType = TokenType::SYMBOL;
        input.get(currentSymbol);
        return;
    }

    // ������������� ��� �������� �����
    if (isalpha(c) || c == '_') {
        std::string token;
        while (isalnum(input.peek()) || input.peek() == '_') {
            token += input.get();
        }
        token += input.get(); // ��������� ������

        if (isKeyword(token)) {
            currentType = TokenType::KEYWORD;
            currentKeyword = keywordMap.at(token);
        }
        else {
            currentType = TokenType::IDENTIFIER;
            currentIdentifier = token;
        }
        return;
    }

    throw std::runtime_error("Invalid character: " + std::string(1, c));
}
std::string keywordToString(Keyword keyword) {
    static const std::unordered_map<Keyword, std::string> map = {
        {Keyword::CLASS, "class"},
        {Keyword::METHOD, "method"},
        {Keyword::FUNCTION, "function"},
        {Keyword::CONSTRUCTOR, "constructor"},
        {Keyword::INT, "int"},
        {Keyword::BOOLEAN, "boolean"},
        {Keyword::CHAR, "char"},
        {Keyword::VOID, "void"},
        {Keyword::VAR, "var"},
        {Keyword::STATIC, "static"},
        {Keyword::FIELD, "field"},
        {Keyword::LET, "let"},
        {Keyword::DO, "do"},
        {Keyword::IF, "if"},
        {Keyword::ELSE, "else"},
        {Keyword::WHILE, "while"},
        {Keyword::RETURN, "return"},
        {Keyword::TRUE, "true"},
        {Keyword::FALSE, "false"},
        {Keyword::NULL_, "null"},  // �������� �������� �� NULL_ ������ null
        {Keyword::THIS, "this"}
    };

    try {
        return map.at(keyword);
    }
    catch (const std::out_of_range&) {
        throw std::runtime_error("Unknown keyword");
    }
}

bool JackTokenizer::isKeyword(const std::string& token) const {
    return keywordMap.count(token);
}
void JackTokenizer::getCurrentTokenInfo() const {

    // ������� �����
    std::cout << "[DEBUG] Line " << lineNumber << " | ";

    // ��������� ������� ���� ������
    switch (currentType) {
    case TokenType::KEYWORD:
        std::cout << "KEYWORD: " << keywordToString(currentKeyword);
        break;

    case TokenType::SYMBOL:
        std::cout << "SYMBOL: '" << std::string(1, currentSymbol) << "'"; // ����� �������� ������
        break;

    case TokenType::IDENTIFIER:
        std::cout << "IDENTIFIER: " << currentIdentifier; // ��� std::string operator<< ��� ���������
        break;

    case TokenType::INT_CONST:
        std::cout << "INT_CONST: " << currentInt;
        break;

    case TokenType::STRING_CONST:
        std::cout << "STRING_CONST: \"" << currentString << "\"";
        break;

    default:
        std::cout << "UNKNOWN";
    }

    // ��������� ����� ������ ���������
    std::cout << " (processed at line " << lineNumber << ")";

    return;
}
// �������
TokenType JackTokenizer::tokenType() const { return currentType; }
Keyword JackTokenizer::keyWord() const { return currentKeyword; }
char JackTokenizer::symbol() const { return currentSymbol; }
std::string JackTokenizer::identifier() const { return currentIdentifier; }
int JackTokenizer::intVal() const { return currentInt; }
std::string JackTokenizer::stringVal() const { return currentString; }