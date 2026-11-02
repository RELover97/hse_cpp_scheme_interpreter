// Общая реализация лексического анализа: используется всеми пятью этапами.

#pragma once

#include <tokenizer.h>
#include <error.h>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>

namespace {

bool SymbolStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) ||
           ch == '<' || ch == '=' || ch == '>' || ch == '*' || ch == '/' || ch == '#';
}

bool SymbolContinue(char ch) {
    return SymbolStart(ch) || std::isdigit(static_cast<unsigned char>(ch)) ||
           ch == '?' || ch == '!' || ch == '-';
}

}  // namespace

bool SymbolToken::operator==(const SymbolToken& rhs) const { 
    return name == rhs.name; 
}

bool QuoteToken::operator==(const QuoteToken&) const { 
    return true; 
}

bool DotToken::operator==(const DotToken&) const {
    return true; 
}

bool ConstantToken::operator==(const ConstantToken& rhs) const { 
    return value == rhs.value; 
}

Tokenizer::Tokenizer(std::istream* in) : in_(in) {
    if (!in_) { 
        throw SyntaxError("null input stream"); 
    }

    Next(); // Инвариант: текущий токен уже прочитан после конструктора
}

// Достигли мы конца потока или нет
bool Tokenizer::IsEnd() { 
    return end_; 
}

// Получить текущий токен
Token Tokenizer::GetToken() {
    if (end_) { throw SyntaxError("no current token"); }
    return token_; // Возвращаем копию: повторный GetToken() ничего не потребляет.
}

// Попытаться прочитать следующий токен
// Либо IsEnd() станет true, либо токен можно будет получить через GetToken()
void Tokenizer::Next() {
    // не читаем весь поток заранее: важен потоковый интерфейс
    while (in_->peek() != EOF &&
           std::isspace(static_cast<unsigned char>(in_->peek()))) {
        in_->get();
    }

    int p = in_->peek();
    if (p == EOF) { 
        end_ = true; 
        return; 
    }

    end_ = false;
    char c = static_cast<char>(in_->get());
    if (c == '(' || c == ')') { 
        token_ = c == '(' ? BracketToken::OPEN : BracketToken::CLOSE;
        return; 
    }

    if (c == '\'') { 
        token_ = QuoteToken{}; 
        return; 
    }
    if (c == '.') { 
        token_ = DotToken{}; 
        return; 
    }

    // + и - — часть числа только при непосредственно следующей цифре.
    const bool digit = std::isdigit(static_cast<unsigned char>(c));
    const bool sign = (c == '+' || c == '-');
    const bool signed_number = sign && in_->peek() != EOF &&
        std::isdigit(static_cast<unsigned char>(in_->peek()));

    if (digit || signed_number) {
        std::string lexeme(1, c);
        while (in_->peek() != EOF &&
               std::isdigit(static_cast<unsigned char>(in_->peek()))) {
            lexeme += static_cast<char>(in_->get());
        }
        try {
            size_t read = 0;
            int64_t value = std::stoll(lexeme, &read);
            if (read != lexeme.size()) { throw SyntaxError("invalid integer"); }
            token_ = ConstantToken{value};
        } catch (const std::out_of_range&) {
            throw SyntaxError("integer is out of int64_t range");
        } catch (const std::invalid_argument&) {
            throw SyntaxError("invalid integer");
        }
        return;
    }

    if (sign) { 
        token_ = SymbolToken{std::string(1, c)}; 
        return; 
    }

    if (!SymbolStart(c)) { 
        throw SyntaxError("unexpected character in input"); 
    }

    std::string name(1, c);
    while (in_->peek() != EOF && SymbolContinue(static_cast<char>(in_->peek()))) {
        name += static_cast<char>(in_->get());
    }
    token_ = SymbolToken{std::move(name)};
}
