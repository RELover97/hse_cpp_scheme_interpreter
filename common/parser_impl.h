// Рекурсивный спуск: атом / список / цитирование.
#pragma once
#include <parser.h>
#include <error.h>
#include <vector>

namespace {

Value ReadList(Tokenizer* tokens) {
    // Открывающая '(' уже была прочитана и потреблена функцией Read().
    std::vector<Value> elements;
    while (!tokens->IsEnd()) {
        const Token t = tokens->GetToken();
        if (const auto* bracket = std::get_if<BracketToken>(&t)) {
            if (*bracket == BracketToken::CLOSE) {
                tokens->Next();
                Value list;
                // Строим список с хвоста, поэтому порядок элементов сохраняется.
                for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
                    list = std::make_shared<Cell>(*it, list);
                }
                return list;
            }
        }

        if (std::holds_alternative<DotToken>(t)) {
            // Точка допустима только после хотя бы одного элемента и только перед хвостом.
            if (elements.empty()) { throw SyntaxError("dot without a head"); }
            tokens->Next();
            Value tail = Read(tokens);
            if (tokens->IsEnd() || tokens->GetToken() != Token{BracketToken::CLOSE}) {
                throw SyntaxError("expected ')' after dotted tail");
            }
            tokens->Next();
            for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
                tail = std::make_shared<Cell>(*it, tail);
            }
            return tail;
        }
        elements.push_back(Read(tokens));
    }
    throw SyntaxError("unclosed list");
}

} // namespace

Value Read(Tokenizer* tokens) 
{
    if (!tokens || tokens->IsEnd()) { 
        throw SyntaxError("expected expression"); 
    }

    Token token = tokens->GetToken();

    tokens->Next(); // Ровно один токен потреблён — следующий доступен для просмотра.
    if (auto* number = std::get_if<ConstantToken>(&token)) {
        return std::make_shared<Number>(number->value);
    }

    if (auto* symbol = std::get_if<SymbolToken>(&token)) {
        if (symbol->name == "#t") { 
            return std::make_shared<Boolean>(true); 
        }
        if (symbol->name == "#f") { 
            return std::make_shared<Boolean>(false); 
        }
        return std::make_shared<Symbol>(symbol->name);
    }

    if (auto* bracket = std::get_if<BracketToken>(&token)) {
        if (*bracket == BracketToken::OPEN) { 
            return ReadList(tokens); 
        }
        throw SyntaxError("unexpected ')'");
    }

    if (std::holds_alternative<QuoteToken>(token)) {
        // 'x — синтаксический сахар для (quote x).
        Value quoted = Read(tokens);
        return std::make_shared<Cell>(std::make_shared<Symbol>("quote"),
                   std::make_shared<Cell>(quoted, nullptr));
    }

    throw SyntaxError("unexpected dot");
}
