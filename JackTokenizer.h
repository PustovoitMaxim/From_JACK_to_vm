#pragma once
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

enum class TokenType {
    KEYWORD, SYMBOL, IDENTIFIER,
    INT_CONST, STRING_CONST, UNKNOWN
};

enum class Keyword {
    CLASS, METHOD, FUNCTION, CONSTRUCTOR,
    INT, BOOLEAN, CHAR, VOID, VAR, STATIC,
    FIELD, LET, DO, IF, ELSE, WHILE, RETURN,
    TRUE, FALSE, NULL_, THIS
};
std::string keywordToString(Keyword keyword);
class JackTokenizer {
public:
    explicit JackTokenizer(const std::string& filename);
    ~JackTokenizer();

    bool hasMoreTokens() const;
    void advance();
    void getCurrentTokenInfo() const;
    TokenType tokenType() const;
    Keyword keyWord() const;
    std::string symbol() const;
    std::string identifier() const;
    int intVal() const;
    std::string stringVal() const;
    void setDebugMode(bool mode);

    std::string currentString;

    void readString();
    void readNumber();
    void readKeywordOrIdentifier();
    //void readSymbol();


private:
    void skipCommentsAndWhitespace();
    void readNextToken();
    bool isKeyword(const std::string& token) const;

    std::ifstream input;
    TokenType currentType;
    Keyword currentKeyword;
    std::string currentSymbol;
    std::string currentIdentifier;
    int currentInt;

    bool debugMode = true;
    size_t lineNumber = 1;

    static const std::unordered_set<char> symbols;
    static const std::unordered_map<std::string, Keyword> keywordMap;
};