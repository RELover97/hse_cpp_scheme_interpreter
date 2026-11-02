#pragma once
#include <memory>
#include "object.h"
#include <tokenizer.h>

// Считывает одно выражение; пустой список представлен nullptr.
std::shared_ptr<Object> Read(Tokenizer* tokenizer);
