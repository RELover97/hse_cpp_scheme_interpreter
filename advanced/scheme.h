#pragma once
#include <memory>
#include <string>
class Runtime;
class Interpreter {
public:
    Interpreter();
    ~Interpreter();
    Interpreter(const Interpreter&) = delete;
    Interpreter& operator=(const Interpreter&) = delete;
    std::string Run(const std::string& source);
private:
    std::unique_ptr<Runtime> impl_;
};
