#include <tokenizer.h>
#include <error.h>
#include <iostream>
#include <sstream>
#include <type_traits>

int main(int argc, char** argv) 
{
    const std::string source = argc > 1 ? argv[1] : "'(+ 2 (/ -3 +4))";

    std::istringstream input(source);
    try {
        Tokenizer tokenizer(&input);
        while (!tokenizer.IsEnd()) {
            const Token token = tokenizer.GetToken();

            std::visit([](const auto& t) {
                using T = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<T, ConstantToken>) {
                    std::cout << "Constant(" << t.value << ")\n";
                } else if constexpr (std::is_same_v<T, SymbolToken>) {
                    std::cout << "Symbol(" << t.name << ")\n";
                } else if constexpr (std::is_same_v<T, BracketToken>) {
                    std::cout << (t == BracketToken::OPEN ? "OpenParen" : "CloseParen") << '\n';
                } else if constexpr (std::is_same_v<T, QuoteToken>) {
                    std::cout << "Quote\n";
                } else {
                    std::cout << "Dot\n";
                }
            }, token);
            tokenizer.Next();
        }
    } catch (const SyntaxError& e) {
        std::cerr << "SyntaxError: " << e.what() << '\n';
        return 1;
    }
}
