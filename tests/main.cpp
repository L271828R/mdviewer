// tests/main.cpp
// Run with: cmake --build build --target test_mdviewer && ./build/test_mdviewer

#include <iostream>

int test_markdown();
int test_html_template();
int test_mdviewer();

int main() {
    int failures = 0;
    failures += test_markdown();
    failures += test_html_template();
    failures += test_mdviewer();
    std::cout << (failures == 0 ? "ALL PASSED" : "FAILED") << "\n";
    return failures > 0 ? 1 : 0;
}
