#include <parser.h>
#include <error.h>
#include <iostream>
#include <sstream>

// Демонстрационный обход синтаксического дерева. Это НЕ вычисление Scheme.
void ShowTree(const Value& root, int depth = 0) {
    const std::string indent(depth * 2, ' ');
    if (!root) { std::cout << indent << "nil\n"; return; }
    if (const auto x = As<Number>(root)) {
        std::cout << indent << "Number " << x->GetValue() << '\n';
    } else if (const auto x = As<Boolean>(root)) {
        std::cout << indent << "Boolean " << (x->GetValue() ? "#t" : "#f") << '\n';
    } else if (const auto x = As<Symbol>(root)) {
        std::cout << indent << "Symbol " << x->GetName() << '\n';
    } else if (const auto x = As<Cell>(root)) {
        std::cout << indent << "Cell\n";
        ShowTree(x->GetFirst(), depth + 1);
        ShowTree(x->GetSecond(), depth + 1);
    }
}
int main(int argc, char** argv) {
    const std::string source = argc > 1 ? argv[1] : "(+ 2 (/ -3 +4))";
    std::istringstream input(source);
    try {
        Tokenizer tokenizer(&input);
        const auto tree = Read(&tokenizer);
        if (!tokenizer.IsEnd()) { throw SyntaxError("extra expression"); }
        ShowTree(tree);
    } catch (const SyntaxError& e) {
        std::cerr << "SyntaxError: " << e.what() << '\n';
        return 1;
    }
}
