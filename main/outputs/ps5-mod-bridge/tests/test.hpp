#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace test {
struct Case { std::string name; std::function<void()> run; };
inline std::vector<Case>& cases() { static std::vector<Case> value; return value; }
struct Register {
    Register(std::string name, std::function<void()> run) {
        cases().push_back({std::move(name), std::move(run)});
    }
};
inline void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                                 ": check failed: " + expression);
    }
}
template <typename A, typename B>
void equal(const A& actual, const B& expected, const char* expression,
           const char* file, int line) {
    if (!(actual == expected)) {
        std::ostringstream out;
        out << file << ':' << line << ": " << expression << "\n  actual: "
            << actual << "\nexpected: " << expected;
        throw std::runtime_error(out.str());
    }
}
struct TempDir {
    std::filesystem::path path;
    TempDir() {
        const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            path = std::filesystem::temp_directory_path() /
                ("modbridge-test-" + std::to_string(seed) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(path)) return;
        }
        throw std::runtime_error("cannot create isolated test directory");
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    ~TempDir() { std::error_code error; std::filesystem::remove_all(path, error); }
};
} // namespace test

#define TEST(name) static void name(); \
    static ::test::Register register_##name(#name, name); static void name()
#define CHECK(expression) ::test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) ::test::equal((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)
#define CHECK_THROWS(expression) do { bool caught_ = false; \
    try { (void)(expression); } catch (...) { caught_ = true; } CHECK(caught_); } while (false)
#define CHECK_THROWS_AS(expression, type) do { bool caught_ = false; \
    try { (void)(expression); } catch (const type&) { caught_ = true; } CHECK(caught_); } while (false)
