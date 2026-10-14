#include "test.hpp"
#include <iostream>

int main() {
    unsigned failures = 0;
    for (const auto& item : test::cases()) {
        try { item.run(); std::cout << "PASS " << item.name << '\n'; }
        catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << item.name << ": " << error.what() << '\n';
        }
        catch (...) { ++failures; std::cerr << "FAIL " << item.name << ": unknown exception\n"; }
    }
    std::cout << test::cases().size() << " tests, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
