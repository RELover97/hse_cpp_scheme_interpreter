// Минимальный автономный runner для проверок из вложенного архива.
// НЕ реализация Catch2: используется только при сборке архива без внешнего курса.
#pragma once
#include <random>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
namespace local_test {
struct Test { const char* name; void (*fn)(); };
inline auto& Registry() { static std::vector<Test> tests; return tests; }
struct Registration { Registration(const char* name, void (*fn)()) { Registry().push_back({name, fn}); } };
inline void Fail(const std::string& message, const char* file, int line) {
    throw std::runtime_error(std::string(file) + ':' + std::to_string(line) + ": " + message);
}
}
#define CAT_A(a, b) a##b
#define CAT(a, b) CAT_A(a, b)
#define TEST_CASE(name) LOCAL_TEST_CASE(name, __LINE__)
#define LOCAL_TEST_CASE(name, line) \
    void CAT(test_case_, line)(); \
    static local_test::Registration CAT(reg_, line)(name, &CAT(test_case_, line)); \
    void CAT(test_case_, line)()
#define TEST_CASE_METHOD(fixture, name) LOCAL_TEST_CASE_METHOD(fixture, name, __LINE__)
#define LOCAL_TEST_CASE_METHOD(fixture, name, line) \
    namespace { \
        struct CAT(test_case_, line) : public fixture { void run(); }; \
        void CAT(run_case_, line)() { CAT(test_case_, line) t; t.run(); } \
        local_test::Registration CAT(reg_, line)(name, &CAT(run_case_, line)); \
    } \
    void CAT(test_case_, line)::run()
#define SECTION(name) if (true)
#define REQUIRE(expr) do { if (!(expr)) { local_test::Fail("REQUIRE(" #expr ")", __FILE__, __LINE__); } } while(0)
#define REQUIRE_NOTHROW(expr) do { try { (void)(expr); } catch (const std::exception& e) { local_test::Fail(std::string("unexpected exception: ") + e.what(), __FILE__, __LINE__); } } while(0)
#define REQUIRE_THROWS_AS(expr, exception_type) do { \
    bool thrown = false; \
    try { (void)(expr); } catch (const exception_type&) { thrown = true; } \
    if (!thrown) { local_test::Fail("expected " #exception_type " from " #expr, __FILE__, __LINE__); } \
} while(0)
