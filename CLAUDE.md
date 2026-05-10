# MDViewer — Claude guidance

## Tests

Always run the test suite **before and after** making any code changes.

```bash
cmake --build build --target test_mdviewer && ./build/test_mdviewer
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
2. **Write a failing test first.** Add the test that exercises the new behaviour before writing any implementation. Confirm the test suite now fails on the new case.
3. Make the change in the appropriate file (not always `mdviewer.cpp`).
4. Run tests again — confirm all tests now pass (including the one you added).
5. Build with `bash build.sh` — zero warnings expected.
