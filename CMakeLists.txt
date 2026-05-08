cmake_minimum_required(VERSION 3.16)
project(mdviewer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base webview)
include(${wxWidgets_USE_FILE})

# ── Mermaid.js — downloaded ONCE at configure time, then bundled offline ─────
set(MERMAID_JS "${CMAKE_CURRENT_SOURCE_DIR}/mermaid.min.js")
if(NOT EXISTS "${MERMAID_JS}")
    message(STATUS "Downloading mermaid.min.js (one-time only — bundled into binary for offline use)…")
    file(DOWNLOAD
        "https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js"
        "${MERMAID_JS}"
        SHOW_PROGRESS
        STATUS MERMAID_DL
    )
    list(GET MERMAID_DL 0 MERMAID_DL_CODE)
    if(NOT MERMAID_DL_CODE EQUAL 0)
        message(FATAL_ERROR
            "Failed to download mermaid.min.js (error ${MERMAID_DL_CODE}).\n"
            "Download manually and place at:\n  ${MERMAID_JS}\n"
            "  curl -L https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js"
            " -o \"${MERMAID_JS}\"")
    endif()
endif()

# Embed mermaid.min.js as a C++ byte array using xxd
find_program(XXD xxd REQUIRED)
set(MERMAID_JS_H "${CMAKE_CURRENT_BINARY_DIR}/mermaid_js.h")
add_custom_command(
    OUTPUT  "${MERMAID_JS_H}"
    COMMAND ${XXD} -i -n mermaid_js_data "${MERMAID_JS}" > "${MERMAID_JS_H}"
    DEPENDS "${MERMAID_JS}"
    COMMENT "Embedding mermaid.min.js into binary"
    VERBATIM
)
add_custom_target(gen_mermaid_header ALL DEPENDS "${MERMAID_JS_H}")
# ─────────────────────────────────────────────────────────────────────────────

# ── Highlight.js — downloaded ONCE at configure time, bundled offline ─────────
set(HLJS_JS        "${CMAKE_CURRENT_SOURCE_DIR}/highlight.min.js")
set(HLJS_CSS_LIGHT "${CMAKE_CURRENT_SOURCE_DIR}/highlight-light.min.css")
set(HLJS_CSS_DARK  "${CMAKE_CURRENT_SOURCE_DIR}/highlight-dark.min.css")

foreach(_PAIR IN ITEMS
    "https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js||${HLJS_JS}||highlight.min.js"
    "https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github.min.css||${HLJS_CSS_LIGHT}||highlight-light.min.css"
    "https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github-dark.min.css||${HLJS_CSS_DARK}||highlight-dark.min.css"
)
    string(REPLACE "||" ";" _P "${_PAIR}")
    list(GET _P 0 _URL)
    list(GET _P 1 _DEST)
    list(GET _P 2 _LABEL)
    if(NOT EXISTS "${_DEST}")
        message(STATUS "Downloading ${_LABEL} (one-time only — bundled into binary for offline use)…")
        file(DOWNLOAD "${_URL}" "${_DEST}" SHOW_PROGRESS STATUS _DL)
        list(GET _DL 0 _DL_CODE)
        if(NOT _DL_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ${_LABEL} (error ${_DL_CODE}).")
        endif()
    endif()
endforeach()

set(HLJS_JS_H        "${CMAKE_CURRENT_BINARY_DIR}/hljs_js.h")
set(HLJS_CSS_LIGHT_H "${CMAKE_CURRENT_BINARY_DIR}/hljs_css_light.h")
set(HLJS_CSS_DARK_H  "${CMAKE_CURRENT_BINARY_DIR}/hljs_css_dark.h")

add_custom_command(OUTPUT "${HLJS_JS_H}"
    COMMAND ${XXD} -i -n hljs_js_data "${HLJS_JS}" > "${HLJS_JS_H}"
    DEPENDS "${HLJS_JS}" COMMENT "Embedding highlight.min.js" VERBATIM)
add_custom_command(OUTPUT "${HLJS_CSS_LIGHT_H}"
    COMMAND ${XXD} -i -n hljs_css_light_data "${HLJS_CSS_LIGHT}" > "${HLJS_CSS_LIGHT_H}"
    DEPENDS "${HLJS_CSS_LIGHT}" COMMENT "Embedding highlight-light.min.css" VERBATIM)
add_custom_command(OUTPUT "${HLJS_CSS_DARK_H}"
    COMMAND ${XXD} -i -n hljs_css_dark_data "${HLJS_CSS_DARK}" > "${HLJS_CSS_DARK_H}"
    DEPENDS "${HLJS_CSS_DARK}" COMMENT "Embedding highlight-dark.min.css" VERBATIM)

add_custom_target(gen_hljs_headers ALL
    DEPENDS "${HLJS_JS_H}" "${HLJS_CSS_LIGHT_H}" "${HLJS_CSS_DARK_H}")
# ─────────────────────────────────────────────────────────────────────────────

set(SOURCES main.cpp mdviewer.cpp)
add_executable(mdviewer ${SOURCES})
add_dependencies(mdviewer gen_mermaid_header gen_hljs_headers)

target_include_directories(mdviewer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(mdviewer PRIVATE ${wxWidgets_LIBRARIES})

if(APPLE)
    find_library(WEBKIT_FW WebKit)
    if(WEBKIT_FW)
        target_link_libraries(mdviewer PRIVATE ${WEBKIT_FW})
    endif()
endif()

install(TARGETS mdviewer DESTINATION bin)
install(FILES completions/_mdviewer
    DESTINATION share/zsh/site-functions)
