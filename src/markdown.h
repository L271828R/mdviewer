#pragma once
#include <string>

// Pure functions — no wxWidgets dependency, directly unit-testable.

std::string EscapeHTML(const std::string& text);
std::string ProcessInline(const std::string& text);
std::string RenderMarkdown(const std::string& md);

// Returns a markdown document describing every syntax feature MDViewer
// supports. Printed to stdout when the binary is invoked with --llm.
std::string GetLLMReadme();
