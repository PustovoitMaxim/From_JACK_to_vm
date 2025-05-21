#include "JackTokenizer.h"
#include <cctype>
#include <stdexcept>
#include <iostream>
#include <algorithm>
// Символы языка Jack
const std::unordered_set<char> JackTokenizer::symbols = {
	'{', '}', '(', ')', '[', ']', '.', ',', ';', '+',
	'-', '*', '/', '&', '|', '<', '>', '=', '~'
};

// Соответствие строк ключевым словам
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
	currentSymbol.clear();
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
	input.get(); // Пропускаем открывающую кавычку "
	currentString.clear();
	currentType = TokenType::STRING_CONST;

	while (!input.eof()) {
		char c = input.get();
		// Запрещаем переносы строк внутри строки
		if (c == '\n') {
			throw std::runtime_error("Newline in string literal at line " + std::to_string(lineNumber));
		}
		// Закрывающая кавычка
		if (c == '"') {
			return;
		}
		currentString += c;
	}
	// Если дошли до конца файла без закрывающей кавычки
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

	// Считываем символы, пока они являются допустимыми для идентификатора
	while (!input.eof() && (isalnum(input.peek()) || input.peek() == '_')) {
		token += input.get();
	}

	// Преобразуем в нижний регистр вручную
	std::string lowerToken;
	for (char c : token) {
		// Безопасное преобразование для tolower()
		lowerToken.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}

	// Проверяем, является ли токен ключевым словом
	if (keywordMap.find(lowerToken) != keywordMap.end()) {
		currentType = TokenType::KEYWORD;
		currentKeyword = keywordMap.at(lowerToken);
	}
	else {
		currentIdentifier = token;
	}
}
//void JackTokenizer::readSymbol() {
//    currentSymbol = input.get();
//    currentType = TokenType::SYMBOL;
//
//    // Проверка на недопустимые символы
//    static const std::unordered_set<char> validSymbols = {
//        '{', '}', '(', ')', '[', ']', '.', ',', ';', '+',
//        '-', '*', '/', '&', '|', '<', '>', '=', '~'
//    };
//
//    if (!validSymbols.count(currentSymbol)) {
//        throw std::runtime_error("Invalid symbol: " +currentSymbol);
//    }
//}
void JackTokenizer::advance() {
	skipCommentsAndWhitespace();
	if (input.eof()) {
		currentType = TokenType::UNKNOWN;
		return;
	}

	size_t tokenLine = lineNumber;
	char c = input.peek();
	// Обработка одиночных символов
	if (symbols.count(c)) {
		currentSymbol = std::string(1, input.get()); // Теперь строка
		if (c == '<' || c == '>') {
			if (input.peek() == '=') {
				currentSymbol += std::string(1, input.get()); // Добавляем "=" → "<=" или ">="
			}
		}
		currentType = TokenType::SYMBOL;
	}
	else if (c == '"') {
		readString();
		currentType = TokenType::STRING_CONST;
	}
	else if (isdigit(c)) {
		readNumber();
		currentType = TokenType::INT_CONST;
	}
	else if (isalpha(c) || c == '_') {
		readKeywordOrIdentifier();
	}
	else {
		throw std::runtime_error("Invalid character '" + std::string(1, c) +
			"' at line " + std::to_string(tokenLine));
	}

	// Отладочный вывод
	if (debugMode) {
		getCurrentTokenInfo();
		std::cout << " (processed at line " << tokenLine << ")" << std::endl;
	}
}
void JackTokenizer::skipCommentsAndWhitespace() {
	bool inComment = false;

	while (!input.eof()) {
		char c = input.peek();

		// Обработка перевода строки
		if (c == '\n') {
			lineNumber++;
			input.get();
			continue;
		}

		// Пропускаем пробелы и табы
		if (isspace(c)) {
			input.get();
			continue;
		}

		// Проверяем начало комментария
		if (!inComment && c == '/') {
			input.get(); // Съедаем '/'
			char next = input.peek();

			// Однострочный комментарий
			if (next == '/') {
				input.get(); // Съедаем второй '/'
				// Пропускаем до конца строки
				while (input.peek() != '\n' && !input.eof()) input.get();
				continue;
			}

			// Многострочный комментарий
			else if (next == '*') {
				input.get(); // Съедаем '*'
				inComment = true;
				bool prevStar = false;

				while (!input.eof()) {
					char commentChar = input.get();

					// Учитываем переводы строк в комментариях
					if (commentChar == '\n') lineNumber++;

					// Проверяем окончание комментария
					if (prevStar && commentChar == '/') {
						inComment = false;
						break;
					}
					prevStar = (commentChar == '*');
				}
				continue;
			}

			// Не комментарий - возвращаем '/' обратно в поток
			else {
				input.putback('/');
				break;
			}
		}

		// Выходим из цикла, если не комментарий и не пробел
		if (!inComment && !isspace(c)) break;

		input.get(); // Продолжаем обработку
	}
}
void JackTokenizer::readNextToken() {
	char c = input.peek();

	// Строковая константа
	if (c == '"') {
		input.get();
		currentType = TokenType::STRING_CONST;
		while (input.get(c) && c != '"') {
			currentString += c;
		}
		return;
	}

	// Числовая константа
	if (isdigit(c)) {
		currentType = TokenType::INT_CONST;
		std::string num;
		while (isdigit(input.peek())) {
			num += input.get();
		}
		currentInt = std::stoi(num);
		return;
	}

	// Символ
	if (symbols.count(c)) {
		currentType = TokenType::SYMBOL;
		char c = input.get(); // Читаем символ в char
		currentSymbol = std::string(1, c); // Преобразуем в строку
		return;
	}

	// Идентификатор или ключевое слово
	if (isalpha(c) || c == '_') {
		std::string token;
		while (isalnum(input.peek()) || input.peek() == '_') {
			token += input.get();
		}
		token += input.get(); // Последний символ

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
		{Keyword::NULL_, "null"},  // Обратите внимание на NULL_ вместо null
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

	// Базовый вывод
	std::cout << "[DEBUG] Line " << lineNumber << " | ";

	// Обработка каждого типа токена
	switch (currentType) {
	case TokenType::KEYWORD:
		std::cout << "KEYWORD: " << keywordToString(currentKeyword);
		break;

	case TokenType::SYMBOL:
		std::cout << "SYMBOL: '" << currentSymbol << "'"; // Явное создание строки
		break;

	case TokenType::IDENTIFIER:
		std::cout << "IDENTIFIER: " << currentIdentifier; // Для std::string operator<< уже определен
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

	// Добавляем номер строки обработки
	std::cout << " (processed at line " << lineNumber << ")";

	return;
}
// Геттеры
TokenType JackTokenizer::tokenType() const { return currentType; }
Keyword JackTokenizer::keyWord() const { return currentKeyword; }
std::string JackTokenizer::symbol() const { return currentSymbol; }
std::string JackTokenizer::identifier() const { return currentIdentifier; }
int JackTokenizer::intVal() const { return currentInt; }
std::string JackTokenizer::stringVal() const { return currentString; }