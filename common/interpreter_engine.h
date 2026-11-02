// Опорная реализация интерпретатора Scheme для basic / advanced / tidy.
// Время жизни объектов отслеживается через shared_ptr, а в tidy дополнительно
// используется обход достижимости: циклы из shared_ptr сами не освобождаются.
#pragma once
#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <error.h>
#include <parser.h>

class Environment : public GcMember {
public:
    explicit Environment(std::shared_ptr<Environment> parent = {}) : parent_(std::move(parent)) {}
    Value Lookup(const std::string& key) const {
        auto it = values_.find(key);
        if (it != values_.end()) { return it->second; }
        if (parent_) { return parent_->Lookup(key); }
        throw NameError("unknown identifier: " + key);
    }
    void Define(const std::string& key, Value value) { values_[key] = std::move(value); }
    void Set(const std::string& key, Value value) {
        auto it = values_.find(key);
        if (it != values_.end()) { it->second = std::move(value); return; }
        if (parent_) { parent_->Set(key, std::move(value)); return; }
        throw NameError("unknown identifier: " + key);
    }
    void Trace(const std::function<void(const std::shared_ptr<GcMember>&)>& visit) const override {
        if (parent_) { visit(parent_); }
        for (const auto& [name, value] : values_) {
            if (value) { visit(value); }
        }
    }
    void DropReferences() override { parent_.reset(); values_.clear(); }
private:
    std::shared_ptr<Environment> parent_;
    std::unordered_map<std::string, Value> values_;
};

class Runtime;
using NativeFunction = std::function<Value(const std::vector<Value>&, Runtime&)>;
class Native : public Object {
public:
    explicit Native(NativeFunction call) : call_(std::move(call)) {}
    Value Call(const std::vector<Value>& args, Runtime& machine) const { return call_(args, machine); }
private:
    NativeFunction call_;
};
class Closure : public Object {
public:
    Closure(std::vector<std::string> args, std::vector<Value> body,
            std::shared_ptr<Environment> scope)
        : args_(std::move(args)), body_(std::move(body)), scope_(std::move(scope)) {}
    const std::vector<std::string>& Params() const { return args_; }
    const std::vector<Value>& Body() const { return body_; }
    std::shared_ptr<Environment> Scope() const { return scope_; }
    void Trace(const std::function<void(const std::shared_ptr<GcMember>&)>& visit) const override {
        if (scope_) { visit(scope_); }
        for (const auto& expr : body_) { if (expr) { visit(expr); } }
    }
    void DropReferences() override { scope_.reset(); body_.clear(); }
private:
    std::vector<std::string> args_;
    std::vector<Value> body_;
    std::shared_ptr<Environment> scope_;
};

class GarbageCollector {
public:
    // weak_ptr не удерживает объект в памяти. Храним ВСЕ созданные объекты,
    // включая временные окружения и промежуточные значения.
    void Track(const std::shared_ptr<GcMember>& obj) {
        tracked_.push_back(obj);
    }
    void Collect(const std::shared_ptr<Environment>& root) {
        std::unordered_set<const GcMember*> reachable;
        std::vector<std::shared_ptr<GcMember>> pending;
        pending.push_back(root);
        while (!pending.empty()) {
            auto current = std::move(pending.back());
            pending.pop_back();
            if (!current || !reachable.insert(current.get()).second) { continue; }
            current->Trace([&pending](const std::shared_ptr<GcMember>& child) {
                pending.push_back(child);
            });
        }
        // Сначала разрушаем рёбра внутри недостижимого подграфа —
        // это разрушает циклы вида environment -> closure -> environment.
        for (auto& item : tracked_) {
            if (auto object = item.lock()) {
                if (!reachable.contains(object.get())) { object->DropReferences(); }
            }
        }
        // Временные weak_ptr удаляем, чтобы таблица не росла бесконечно.
        tracked_.erase(std::remove_if(tracked_.begin(), tracked_.end(),
                [](const auto& item) { return item.expired(); }), tracked_.end());
    }
private:
    std::vector<std::weak_ptr<GcMember>> tracked_;
};

class Runtime {
public:
    explicit Runtime(bool advanced, bool tidy) : advanced_(advanced), tidy_(tidy) {
        global_ = Make<Environment>();
        InstallBuiltins();
    }
    ~Runtime() {
        // Global может входить в цикл с замыканием, поэтому при завершении
        // работы тоже разрываем ссылки для корректного освобождения.
        if (tidy_ && global_) {
            global_->DropReferences();
            gc_.Collect(global_);
        }
    }
    template<class T, class... Args>
    std::shared_ptr<T> Make(Args&&... args) {
        auto node = std::make_shared<T>(std::forward<Args>(args)...);
        if (tidy_) { gc_.Track(node); }
        return node;
    }
    Value Num(int64_t x) { return Make<Number>(x); }
    Value Bool(bool b) { return Make<Boolean>(b); }
    std::string Run(const std::string& source) {
        // Каждая команда должна состоять ровно из одного выражения.
        std::stringstream input(source);
        try {
            Tokenizer tokenizer(&input);
            Value expr = Read(&tokenizer);
            if (!tokenizer.IsEnd()) { throw SyntaxError("unexpected tokens after expression"); }
            if (tidy_) { TrackAst(expr); }
            Value result = Eval(expr, global_);
            std::string answer = Print(result);
            result.reset();
            expr.reset();
            if (tidy_) { gc_.Collect(global_); } // До возврата из Run!
            return answer;
        } catch (...) {
            if (tidy_) { gc_.Collect(global_); }
            throw;
        }
    }
private:
    bool advanced_;
    bool tidy_;
    GarbageCollector gc_;
    std::shared_ptr<Environment> global_;

    void TrackAst(const Value& root) {
        std::unordered_set<const GcMember*> seen;
        std::vector<Value> stack{root};
        while (!stack.empty()) {
            Value value = std::move(stack.back()); stack.pop_back();
            if (!value || !seen.insert(value.get()).second) { continue; }
            gc_.Track(value);
            if (auto cell = As<Cell>(value)) {
                stack.push_back(cell->GetFirst());
                stack.push_back(cell->GetSecond());
            }
        }
    }
    static bool Truth(const Value& value) {
        auto boolean = As<Boolean>(value);
        return !boolean || boolean->GetValue(); // Только #f — ложь, даже () истинно.
    }
    static void RequireArity(const std::vector<Value>& args, size_t expected) {
        if (args.size() != expected) { throw RuntimeError("wrong number of arguments"); }
    }
    static int64_t AsNumber(const Value& value) {
        auto number = As<Number>(value);
        if (!number) { throw RuntimeError("expected integer"); }
        return number->GetValue();
    }
    static std::shared_ptr<Cell> AsCell(const Value& value) {
        auto cell = As<Cell>(value);
        if (!cell) { throw RuntimeError("expected pair"); }
        return cell;
    }
    static std::vector<Value> Elements(Value list) {
        std::vector<Value> result;
        std::unordered_set<const Object*> visited;
        while (list) {
            auto cell = As<Cell>(list);
            if (!cell || !visited.insert(cell.get()).second) {
                throw RuntimeError("expected proper, acyclic list");
            }
            result.push_back(cell->GetFirst());
            list = cell->GetSecond();
        }
        return result;
    }
    static bool ProperList(Value list) {
        std::unordered_set<const Object*> visited;
        while (list) {
            auto cell = As<Cell>(list);
            if (!cell || !visited.insert(cell.get()).second) { return false; }
            list = cell->GetSecond();
        }
        return true;
    }
    static const std::string& ExpectSymbolName(const Value& value) {
        auto symbol = As<Symbol>(value);
        if (!symbol) { throw SyntaxError("expected identifier"); }
        return symbol->GetName();
    }
    static std::vector<std::string> Parameters(const Value& formals) {
        std::vector<std::string> names;
        std::unordered_set<std::string> used;
        for (const Value& arg : Elements(formals)) {
            std::string name = ExpectSymbolName(arg);
            if (!used.insert(name).second) { throw SyntaxError("duplicate parameter"); }
            names.push_back(std::move(name));
        }
        return names;
    }
    Value Eval(const Value& expr, const std::shared_ptr<Environment>& scope) {
        if (!expr) { throw RuntimeError("empty list is not an expression"); }
        if (Is<Number>(expr) || Is<Boolean>(expr) || Is<Native>(expr) || Is<Closure>(expr)) {
            return expr;
        }
        if (auto symbol = As<Symbol>(expr)) { return scope->Lookup(symbol->GetName()); }
        auto cell = As<Cell>(expr);
        if (!cell) { throw RuntimeError("unsupported expression"); }
        // Разбор особых форм до вычисления аргументов — принципиален для if/and/or.
        std::vector<Value> unevaluated = Elements(cell->GetSecond());
        auto head = As<Symbol>(cell->GetFirst());
        const std::string name = head ? head->GetName() : "";
        if (name == "quote") {
            if (unevaluated.size() != 1) { throw SyntaxError("quote needs one expression"); }
            return unevaluated[0];
        }
        if (name == "and" || name == "or") {
            Value result = Bool(name == "and");
            for (const Value& arg : unevaluated) {
                result = Eval(arg, scope);
                if (name == "and" ? !Truth(result) : Truth(result)) { break; }
            }
            return result;
        }
        if (advanced_) {
            if (name == "if") {
                if (unevaluated.size() != 2 && unevaluated.size() != 3) {
                    throw SyntaxError("if needs two or three arguments");
                }
                if (Truth(Eval(unevaluated[0], scope))) { return Eval(unevaluated[1], scope); }
                return unevaluated.size() == 3 ? Eval(unevaluated[2], scope) : Value{};
            }
            if (name == "lambda") {
                if (unevaluated.size() < 2) { throw SyntaxError("lambda needs parameters and body"); }
                auto params = Parameters(unevaluated[0]);
                return Make<Closure>(std::move(params),
                        std::vector<Value>(unevaluated.begin() + 1, unevaluated.end()), scope);
            }
            if (name == "define") {
                if (unevaluated.size() < 2) { throw SyntaxError("define needs name and value"); }
                if (auto var = As<Symbol>(unevaluated[0])) {
                    if (unevaluated.size() != 2) { throw SyntaxError("bad define"); }
                    Value value = Eval(unevaluated[1], scope); // До внесения имени.
                    scope->Define(var->GetName(), value);
                } else if (auto fn = As<Cell>(unevaluated[0])) {
                    std::string function_name = ExpectSymbolName(fn->GetFirst());
                    auto params = Parameters(fn->GetSecond());
                    scope->Define(function_name, Make<Closure>(std::move(params),
                        std::vector<Value>(unevaluated.begin() + 1, unevaluated.end()), scope));
                } else {
                    throw SyntaxError("bad define target");
                }
                return nullptr;
            }
            if (name == "set!") {
                if (unevaluated.size() != 2) { throw SyntaxError("bad set!"); }
                std::string var = ExpectSymbolName(unevaluated[0]);
                Value value = Eval(unevaluated[1], scope);
                scope->Set(var, value); // Не создаёт новую переменную.
                return nullptr;
            }
            if (name == "set-car!" || name == "set-cdr!") {
                if (unevaluated.size() != 2) { throw SyntaxError("bad pair mutation"); }
                auto pair = AsCell(Eval(unevaluated[0], scope));
                Value replacement = Eval(unevaluated[1], scope);
                if (name == "set-car!") { pair->SetFirst(replacement); }
                else { pair->SetSecond(replacement); }
                return nullptr;
            }
        }
        Value proc = Eval(cell->GetFirst(), scope);
        std::vector<Value> args;
        args.reserve(unevaluated.size());
        for (const Value& arg : unevaluated) { args.push_back(Eval(arg, scope)); }
        if (auto native = As<Native>(proc)) { return native->Call(args, *this); }
        if (auto closure = As<Closure>(proc)) {
            if (args.size() != closure->Params().size()) {
                throw RuntimeError("wrong number of lambda parameters");
            }
            auto call_scope = Make<Environment>(closure->Scope());
            for (size_t i = 0; i < args.size(); ++i) {
                call_scope->Define(closure->Params()[i], args[i]);
            }
            Value result;
            for (const Value& statement : closure->Body()) {
                result = Eval(statement, call_scope);
            }
            return result;
        }
        throw RuntimeError("attempt to call non-function");
    }
    void InstallBuiltins() {
        auto bind = [this](const std::string& name, NativeFunction operation) {
            global_->Define(name, Make<Native>(std::move(operation)));
        };
        bind("number?", [](const auto& a, Runtime& r) { RequireArity(a, 1); return r.Bool(Is<Number>(a[0])); });
        bind("boolean?", [](const auto& a, Runtime& r) { RequireArity(a, 1); return r.Bool(Is<Boolean>(a[0])); });
        bind("symbol?", [](const auto& a, Runtime& r) { RequireArity(a, 1); return r.Bool(Is<Symbol>(a[0])); });
        bind("pair?", [](const auto& a, Runtime& r) { RequireArity(a, 1); return r.Bool(Is<Cell>(a[0])); });
        bind("null?", [](const auto& a, Runtime& r) { RequireArity(a, 1); return r.Bool(!a[0]); });
        bind("list?", [](const auto& a, Runtime& r) { RequireArity(a, 1); return r.Bool(ProperList(a[0])); });
        bind("not", [](const auto& a, Runtime& r) { RequireArity(a, 1); return r.Bool(!Truth(a[0])); });
        bind("cons", [](const auto& a, Runtime& r) { RequireArity(a, 2); return std::static_pointer_cast<Object>(r.Make<Cell>(a[0], a[1])); });
        bind("car", [](const auto& a, Runtime&) { RequireArity(a, 1); return AsCell(a[0])->GetFirst(); });
        bind("cdr", [](const auto& a, Runtime&) { RequireArity(a, 1); return AsCell(a[0])->GetSecond(); });
        bind("list", [](const auto& a, Runtime& r) {
            Value list;
            for (auto it = a.rbegin(); it != a.rend(); ++it) { list = r.Make<Cell>(*it, list); }
            return list;
        });
        bind("list-tail", [](const auto& a, Runtime&) {
            RequireArity(a, 2);
            auto index = AsNumber(a[1]);
            if (index < 0) { throw RuntimeError("negative index"); }
            Value node = a[0];
            for (int64_t i = 0; i < index; ++i) { node = AsCell(node)->GetSecond(); }
            if (node && !Is<Cell>(node)) { throw RuntimeError("not a list"); }
            return node;
        });
        bind("list-ref", [](const auto& a, Runtime&) {
            RequireArity(a, 2);
            auto index = AsNumber(a[1]);
            if (index < 0) { throw RuntimeError("negative index"); }
            Value node = a[0];
            for (int64_t i = 0; i < index; ++i) { node = AsCell(node)->GetSecond(); }
            return AsCell(node)->GetFirst();
        });
        bind("+", [](const auto& a, Runtime& r) {
            int64_t result = 0;
            for (const auto& x : a) { result += AsNumber(x); }
            return r.Num(result);
        });
        bind("*", [](const auto& a, Runtime& r) {
            int64_t result = 1;
            for (const auto& x : a) { result *= AsNumber(x); }
            return r.Num(result);
        });
        bind("-", [](const auto& a, Runtime& r) {
            if (a.empty()) { throw RuntimeError("- needs arguments"); }
            int64_t result = AsNumber(a.front());
            if (a.size() == 1) { return r.Num(-result); }
            for (size_t i = 1; i < a.size(); ++i) { result -= AsNumber(a[i]); }
            return r.Num(result);
        });
        bind("/", [](const auto& a, Runtime& r) {
            if (a.empty()) { throw RuntimeError("/ needs arguments"); }
            int64_t result = AsNumber(a.front());
            for (size_t i = 1; i < a.size(); ++i) {
                int64_t denominator = AsNumber(a[i]);
                if (denominator == 0) { throw RuntimeError("division by zero"); }
                // В задании переполнение исключено; int64_t::min / -1 отдельно не проверяется.
                result /= denominator;
            }
            return r.Num(result);
        });
        bind("abs", [](const auto& a, Runtime& r) {
            RequireArity(a, 1);
            int64_t x = AsNumber(a[0]);
            return r.Num(x < 0 ? -x : x);
        });
        bind("max", [](const auto& a, Runtime& r) {
            if (a.empty()) { throw RuntimeError("max needs arguments"); }
            int64_t result = AsNumber(a[0]);
            for (auto& x : a) { result = std::max(result, AsNumber(x)); }
            return r.Num(result);
        });
        bind("min", [](const auto& a, Runtime& r) {
            if (a.empty()) { throw RuntimeError("min needs arguments"); }
            int64_t result = AsNumber(a[0]);
            for (auto& x : a) { result = std::min(result, AsNumber(x)); }
            return r.Num(result);
        });
        // Все сравнения используют одинаковый алгоритм попарной проверки.
        auto comparison = [bind](std::string name, std::function<bool(int64_t, int64_t)> cmp) {
            bind(name, [cmp](const auto& a, Runtime& r) {
                for (size_t i = 0; i < a.size(); ++i) {
                    int64_t current = AsNumber(a[i]);
                    if (i && !cmp(AsNumber(a[i - 1]), current)) { return r.Bool(false); }
                }
                return r.Bool(true);
            });
        };
        comparison("=", std::equal_to<>{});
        comparison("<", std::less<>{});
        comparison(">", std::greater<>{});
        comparison("<=", std::less_equal<>{});
        comparison(">=", std::greater_equal<>{});
    }
    static std::string Print(const Value& root) {
        std::unordered_set<const Object*> path;
        std::function<std::string(const Value&)> print = [&](const Value& value) -> std::string {
            if (!value) { return "()"; }
            if (auto num = As<Number>(value)) { return std::to_string(num->GetValue()); }
            if (auto boolean = As<Boolean>(value)) { return boolean->GetValue() ? "#t" : "#f"; }
            if (auto symbol = As<Symbol>(value)) { return symbol->GetName(); }
            if (Is<Native>(value) || Is<Closure>(value)) { return "#<procedure>"; }
            auto cell = As<Cell>(value);
            if (!cell) { throw RuntimeError("cannot print object"); }
            if (!path.insert(cell.get()).second) { throw RuntimeError("cannot print cyclic pair"); }
            std::string result = "(";
            // Посещённые ячейки остаются в path на протяжении печати всей ветви.
            Value cursor = value;
            bool first = true;
            std::vector<const Object*> chain;
            while (auto current = As<Cell>(cursor)) {
                if (!first && !path.insert(current.get()).second) {
                    throw RuntimeError("cannot print cyclic list");
                }
                if (first) { first = false; } // Корень уже помещён в path.
                chain.push_back(current.get());
                if (result != "(") { result += " "; }
                result += print(current->GetFirst());
                cursor = current->GetSecond();
            }
            if (cursor) { result += " . " + print(cursor); }
            result += ")";
            for (const auto* node : chain) { path.erase(node); }
            return result;
        };
        return print(root);
    }
};
