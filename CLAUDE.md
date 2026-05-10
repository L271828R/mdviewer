# MDViewer — Claude guidance

## Tests

Always run the test suite **before and after** making any code changes.

```bash
c++ -std=c++17 -o test_dark_mode test_dark_mode.cpp && ./test_dark_mode
```

All tests must pass before committing.

## Code organisation

Split by concern — do not add everything to `mdviewer.cpp`.

| File | Owns |
|---|---|
| `html_template.h/cpp` | `WrapWithTemplate`, CSS/JS generation |
| `markdown.h/cpp` | `RenderMarkdown`, `ProcessInline`, `EscapeHTML` |
| `mdviewer.h/cpp` | Frame class: constructor, event table, file I/O |

Headers are the public interface. A reader should be able to understand a module by reading its `.h` file alone — keep them free of implementation detail.

## C++ style

- Prefer free functions over member functions when a function doesn't need `this`.
- No raw `new`/`delete` — use `wxWeakRef`, stack allocation, or smart pointers.
- Raw string literals (`R"HTML(...)HTML"`) must not be broken mid-tag. If you need to splice a runtime value in, end the literal at a clean boundary (end of attribute value, end of line).
- Keep functions under ~50 lines. If a function is growing, extract a named helper.

## Adding features

1. Run tests — confirm they pass.
2. Make the change in the appropriate file (not always `mdviewer.cpp`).
3. Add or update a test that would have caught a regression.
4. Run tests again — confirm they still pass.
5. Build with `bash build.sh` — zero warnings expected.
