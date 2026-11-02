#include <catch.hpp>
int main() {
    int failures = 0;
    for (const auto& test : local_test::Registry()) {
        try {
            test.fn();
            std::cout << "[ PASS ] " << test.name << '\n';
        } catch (const std::exception& e) {
            std::cerr << "[ FAIL ] " << test.name << ": " << e.what() << '\n';
            ++failures;
        }
    }
    return failures ? 1 : 0;
}
