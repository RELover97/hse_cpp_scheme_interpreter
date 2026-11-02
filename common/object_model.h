#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

// В tidy эти же сущности образуют граф объектов для сборщика мусора.
// В предыдущих частях методы Trace/DropReferences безвредны.
class GcMember {
public:
    virtual ~GcMember() = default;
    virtual void Trace(const std::function<void(const std::shared_ptr<GcMember>&)>&) const {}
    virtual void DropReferences() {}
};

class Object : public GcMember, public std::enable_shared_from_this<Object> {
public:
    ~Object() override = default;
};
using Value = std::shared_ptr<Object>;

class Number : public Object {
public:
    explicit Number(int64_t value) : value_(value) {}
    int64_t GetValue() const { return value_; }
private:
    int64_t value_;
};

class Symbol : public Object {
public:
    explicit Symbol(std::string name) : name_(std::move(name)) {}
    const std::string& GetName() const { return name_; }
private:
    std::string name_;
};

class Boolean : public Object {
public:
    explicit Boolean(bool value) : value_(value) {}
    bool GetValue() const { return value_; }
private:
    bool value_;
};

class Cell : public Object {
public:
    Cell(Value first, Value second) : first_(std::move(first)), second_(std::move(second)) {}
    Value GetFirst() const { return first_; }
    Value GetSecond() const { return second_; }
    void SetFirst(Value value) { first_ = std::move(value); }
    void SetSecond(Value value) { second_ = std::move(value); }
    void Trace(const std::function<void(const std::shared_ptr<GcMember>&)>& visit) const override {
        if (first_) { visit(first_); }
        if (second_) { visit(second_); }
    }
    void DropReferences() override { first_.reset(); second_.reset(); }
private:
    Value first_;
    Value second_;
};

template<class T>
std::shared_ptr<T> As(const Value& object) {
    return std::dynamic_pointer_cast<T>(object);
}

template<class T>
bool Is(const Value& object) {
    return static_cast<bool>(As<T>(object));
}
