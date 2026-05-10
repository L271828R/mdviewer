#pragma once
#include <string>

// Pure functions — no wxWidgets dependency, directly unit-testable.

std::string EscapeHTML(const std::string& text);
std::string ProcessInline(const std::string& text);
std::string RenderMarkdown(const std::string& md);
