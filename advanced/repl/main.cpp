#include <iostream>

#include <error.h>
#include <scheme.h>

int main() {
    Interpreter interpreter;
    std::string query;

    while (true) {
        std::cout << "=> ";
        std::getline(std::cin, query);
        if (std::cin.eof() || query == "q") {
            std::cerr << "Exiting" << std::endl;
            break;
        }

        try {
            auto result = interpreter.Run(query);
            std::cout << "=> " << result << std::endl;
        } catch (const SyntaxError& syntax_error) {
            std::cerr << "Caught SyntaxError: " << syntax_error.what() << std::endl;
        } catch (const NameError& name_error) {
            std::cerr << "Caught NameError: " << name_error.what() << std::endl;
        } catch (const RuntimeError& runtime_error) {
            std::cerr << "Caught RuntimeError: " << runtime_error.what() << std::endl;
        } catch (...) {
            std::cerr << "Caught unknown exception" << std::endl;
        }
    }
}
