#pragma once
#include <cstdint>
#include <istream>
#include <string>
#include <variant>

// У каждого токена ровно один активный вариант (std::variant).
struct SymbolToken { std::string name; bool operator==(const SymbolToken&) const; };
struct QuoteToken { bool operator==(const QuoteToken&) const; };
struct DotToken { bool operator==(const DotToken&) const; };
enum class BracketToken { OPEN, CLOSE };
struct ConstantToken { int64_t value; bool operator==(const ConstantToken&) const; };
using Token = std::variant<ConstantToken, BracketToken, SymbolToken, QuoteToken, DotToken>;

class Tokenizer {
public:
    explicit Tokenizer(std::istream* in);
    bool IsEnd();
    void Next();
    Token GetToken();
private:
    std::istream* in_;
    Token token_ = BracketToken::OPEN;
    bool end_ = false;
};
