// tests/test_renderer.cpp
// Built via CMake: cmake --build build --target test_mdviewer && ./build/test_mdviewer

#include "html_template.h"
#include "markdown.h"
#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // 1. DOCTYPE must be well-formed — a stray ')' puts the browser in quirks
    //    mode, breaking CSS custom-property inheritance and dark-mode colours.
    {
        std::string html = BuildHTML("", "test", false, 100);
        if (html.find("<!DOCTYPE html>") == std::string::npos) {
            std::cerr << "FAIL [doctype]: expected '<!DOCTYPE html>' — got: "
                      << html.substr(0, html.find('\n')) << "\n";
            ++failures;
        } else {
            std::cout << "PASS [doctype]\n";
        }
    }

    // 2. Dark mode: <html class="dark"> must be present so .dark CSS rules fire.
    {
        std::string html = BuildHTML("", "test", true, 100);
        if (html.find("<html lang=\"en\" class=\"dark\">") == std::string::npos) {
            std::cerr << "FAIL [dark-class]: '<html lang=\"en\" class=\"dark\">' not found\n";
            ++failures;
        } else {
            std::cout << "PASS [dark-class]\n";
        }
    }

    // 3. Light mode must NOT carry the dark class.
    {
        std::string html = BuildHTML("", "test", false, 100);
        if (html.find("class=\"dark\"") != std::string::npos) {
            std::cerr << "FAIL [light-no-dark-class]: class=\"dark\" found in light-mode HTML\n";
            ++failures;
        } else {
            std::cout << "PASS [light-no-dark-class]\n";
        }
    }

    // 4. Font size is injected into the CSS.
    {
        std::string html = BuildHTML("", "test", false, 120);
        if (html.find("19.2px") == std::string::npos) {
            std::cerr << "FAIL [font-size]: expected '19.2px' for 120% not found\n";
            ++failures;
        } else {
            std::cout << "PASS [font-size]\n";
        }
    }

    // 5. GetLLMReadme() covers the tidbit extension and core syntax.
    {
        std::string readme = GetLLMReadme();
        bool hasTidbit  = readme.find(":::tidbit") != std::string::npos;
        bool hasMermaid = readme.find("mermaid")   != std::string::npos;
        bool hasHeading = readme.find("# ")        != std::string::npos;
        if (!hasTidbit || !hasMermaid || !hasHeading) {
            std::cerr << "FAIL [llm-readme]: missing tidbit=" << hasTidbit
                      << " mermaid=" << hasMermaid << " heading=" << hasHeading << "\n";
            ++failures;
        } else {
            std::cout << "PASS [llm-readme]\n";
        }
    }

    // 6. :::tidbit[Speaker] block renders as a <details> widget.
    {
        std::string md =
            ":::tidbit[Bjarne Stroustrup]\n"
            "vtables are a feature, not a bug.\n"
            ":::\n";
        std::string html = RenderMarkdown(md);
        bool hasDetails = html.find("<details class=\"tidbit\">") != std::string::npos;
        bool hasSpeaker = html.find("Bjarne Stroustrup") != std::string::npos;
        bool hasContent = html.find("vtables are a feature") != std::string::npos;
        if (!hasDetails || !hasSpeaker || !hasContent) {
            std::cerr << "FAIL [tidbit]: expected <details class=\"tidbit\"> with speaker and content\n"
                      << "  got: " << html << "\n";
            ++failures;
        } else {
            std::cout << "PASS [tidbit]\n";
        }
    }

    std::cout << (failures ? "FAILED" : "ALL PASSED") << "\n";
    return failures > 0 ? 1 : 0;
}
