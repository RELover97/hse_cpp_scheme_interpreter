#include "scheme.h"
#include "../common/interpreter_engine.h"

Interpreter::Interpreter() : impl_(std::make_unique<Runtime>(true,true)) {}
Interpreter::~Interpreter() = default;
std::string Interpreter::Run(const std::string& expression) {
    return impl_->Run(expression);
}
