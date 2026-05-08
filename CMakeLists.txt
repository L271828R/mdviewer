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

set(SOURCES main.cpp mdviewer.cpp)
add_executable(mdviewer ${SOURCES})
add_dependencies(mdviewer gen_mermaid_header)

target_include_directories(mdviewer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(mdviewer PRIVATE ${wxWidgets_LIBRARIES})

if(APPLE)
    find_library(WEBKIT_FW WebKit)
    if(WEBKIT_FW)
        target_link_libraries(mdviewer PRIVATE ${WEBKIT_FW})
    endif()
endif()

install(TARGETS mdviewer DESTINATION bin)
