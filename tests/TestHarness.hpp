#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace TrainSim::Tests {

struct Case { std::string name; std::function<void()> run; };

inline std::vector<Case>& Cases()
{
    static std::vector<Case> cases;
    return cases;
}

struct Register
{
    Register(std::string name, std::function<void()> run) { Cases().push_back({std::move(name), std::move(run)}); }
};

inline void Require(bool condition, const char* expression, const char* file, int line)
{
    if (!condition)
        throw std::runtime_error(std::string(file) + ":" + std::to_string(line)
                                 + ": requirement failed: " + expression);
}

} // namespace TrainSim::Tests

#define TS_JOIN_IMPL(a, b) a##b
#define TS_JOIN(a, b) TS_JOIN_IMPL(a, b)
#define TEST_CASE(name) \
    static void TS_JOIN(test_, __LINE__)(); \
    static TrainSim::Tests::Register TS_JOIN(register_, __LINE__)(name, TS_JOIN(test_, __LINE__)); \
    static void TS_JOIN(test_, __LINE__)()
#define REQUIRE(expression) TrainSim::Tests::Require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
