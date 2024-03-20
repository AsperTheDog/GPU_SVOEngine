# pragma once

#include <functional>
#include <vector>

template<typename... Args>
class Signal {
public:
    using FunctionType = std::function<void(Args...)>;

    void connect(FunctionType func) {
        m_functions.push_back(func);
    }

    template<typename T>
    void connect(T* instance, void (T::*method)(Args...)) {
        m_functions.push_back([=](Args... args) { return (instance->*method)(args...); });
    }

    void emit(Args... args) {
        for (const auto& func : m_functions) {
            func(args...);
        }
    }

    size_t isEmpty() { return m_functions.size(); }

private:
    std::vector<FunctionType> m_functions;
};