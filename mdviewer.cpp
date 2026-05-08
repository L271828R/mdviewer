#include "mdviewer.h"
#include "mermaid_js.h"      // generated at build time: mermaid_js_data[], mermaid_js_data_len
#include "hljs_js.h"         // generated at build time: hljs_js_data[], hljs_js_data_len
#include "hljs_css_light.h"  // generated at build time: hljs_css_light_data[], hljs_css_light_data_len
#include "hljs_css_dark.h"   // generated at build time: hljs_css_dark_data[], hljs_css_dark_data_len
#include <wx/webview.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/config.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <functional>
#include <cctype>
#include <algorithm>

// Return the embedded mermaid.min.js as a string, safe for inline <script> injection.
// Sanitises </script> sequences (HTML parser would close the tag early) once on first call.
static const std::string& GetMermaidJS() {
    static std::string cached;
    if (cached.empty()) {
        cached.assign(reinterpret_cast<const char*>(mermaid_js_data), mermaid_js_data_len);
        // In HTML5, </script> inside a raw-text element ends the block even inside quotes.
        // Replace with <\/script> which is identical at JS runtime.
        const std::string bad  = "</script>";
        const std::string safe = "<\\/script>";
        size_t pos = 0;
        while ((pos = cached.find(bad, pos)) != std::string::npos) {
            cached.replace(pos, bad.size(), safe);
            pos += safe.size();
        }
    }
    return cached;
}

static const std::string& GetHighlightJS() {
    static std::string cached;
    if (cached.empty()) {
        cached.assign(reinterpret_cast<const char*>(hljs_js_data), hljs_js_data_len);
        const std::string bad  = "</script>";
        const std::string safe = "<\\/script>";
        size_t pos = 0;
        while ((pos = cached.find(bad, pos)) != std::string::npos) {
            cached.replace(pos, bad.size(), safe);
            pos += safe.size();
        }
    }
    return cached;
}

static const std::string& GetHighlightCSSLight() {
    static std::string cached;
    if (cached.empty())
        cached.assign(reinterpret_cast<const char*>(hljs_css_light_data), hljs_css_light_data_len);
    return cached;
}

static const std::string& GetHighlightCSSDark() {
    static std::string cached;
    if (cached.empty())
        cached.assign(reinterpret_cast<const char*>(hljs_css_dark_data), hljs_css_dark_data_len);
    return cached;
}

// ---------------------------------------------------------------------------
// Event table
// ---------------------------------------------------------------------------
wxBEGIN_EVENT_TABLE(MDViewerFrame, wxFrame)
    EVT_MENU(wxID_OPEN,      MDViewerFrame::OnOpen)
    EVT_MENU(ID_RELOAD,      MDViewerFrame::OnReload)
    EVT_MENU(ID_THEME_LIGHT, MDViewerFrame::OnThemeLight)
    EVT_MENU(ID_THEME_DARK,  MDViewerFrame::OnThemeDark)
    EVT_MENU(ID_VIEW_LOGS,   MDViewerFrame::OnViewLogs)
    EVT_MENU(ID_VIEW_DOC,    MDViewerFrame::OnViewDoc)
    EVT_MENU(wxID_EXIT,      MDViewerFrame::OnExit)
    EVT_CLOSE(               MDViewerFrame::OnClose)
wxEND_EVENT_TABLE()

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
MDViewerFrame::MDViewerFrame(const wxString& filePath)
    : wxFrame(nullptr, wxID_ANY,
              "MDViewer — " + wxFileName(filePath).GetFullName(),
              wxDefaultPosition, wxSize(1280, 860))
    , m_darkMode(false)
{
    // Always store an absolute path so base URLs resolve correctly in WKWebView
    wxFileName fn(filePath);
    fn.MakeAbsolute();
    m_filePath = fn.GetFullPath();

    // Load persisted theme preference (default: light)
    wxConfig cfg("MDViewer");
    m_darkMode = cfg.ReadBool("darkMode", false);

    // ── File menu ────────────────────────────────────────────────────────
    wxMenuBar* bar  = new wxMenuBar();
    wxMenu*    file = new wxMenu();
    file->Append(wxID_OPEN,  "&Open…\tCtrl+O");
    file->Append(ID_RELOAD,  "&Reload\tCtrl+R");
    file->AppendSeparator();
    file->Append(wxID_EXIT,  "E&xit\tCtrl+Q");
    bar->Append(file, "&File");

    // ── View menu ────────────────────────────────────────────────────────
    wxMenu* view = new wxMenu();
    view->AppendRadioItem(ID_THEME_LIGHT, "&Light Mode\tCtrl+Shift+L");
    view->AppendRadioItem(ID_THEME_DARK,  "&Dark Mode\tCtrl+Shift+D");
    view->Check(m_darkMode ? ID_THEME_DARK : ID_THEME_LIGHT, true);
    view->AppendSeparator();
    view->Append(ID_VIEW_LOGS, "View &Logs\tCtrl+Shift+G");
    view->Append(ID_VIEW_DOC,  "View &Document\tCtrl+Shift+V");
    bar->Append(view, "&View");

    SetMenuBar(bar);

    CreateStatusBar();
    SetStatusText("Loading…");

    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    // Defer so WKWebView finishes initialising before we push content
    CallAfter([this]() { LoadAndRender(); });
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------
std::string MDViewerFrame::ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        Logger::get().log("ReadFile FAILED: " + path);
        return "";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    Logger::get().log("ReadFile OK: " + path + "  (" + std::to_string(content.size()) + " bytes)");
    return content;
}

// ---------------------------------------------------------------------------
// Dispatch: .html → load URL directly; .md → render markdown
// ---------------------------------------------------------------------------
void MDViewerFrame::LoadAndRender() {
    wxString ext = wxFileName(m_filePath).GetExt().Lower();
    Logger::get().log("LoadAndRender: " + m_filePath.ToStdString() + "  ext=" + ext.ToStdString());

    if (ext == "html" || ext == "htm") {
        wxString url = "file://" + m_filePath;
#ifdef __WXMSW__
        url.Replace("/", "\\");
        url = "file:///" + m_filePath;
#endif
        Logger::get().log("LoadURL: " + url.ToStdString());
        m_webView->LoadURL(url);
        SetStatusText("Loaded HTML: " + m_filePath);
        return;
    }

    std::string raw  = ReadFile(m_filePath.ToStdString());
    std::string body = RenderMarkdown(raw);
    Logger::get().log("RenderMarkdown: input=" + std::to_string(raw.size())
                      + " bytes  output=" + std::to_string(body.size()) + " bytes");

    std::string title = wxFileName(m_filePath).GetFullName().ToStdString();
    std::string html  = WrapWithTemplate(body, title, m_darkMode);
    Logger::get().log("WrapWithTemplate: html=" + std::to_string(html.size()) + " bytes");

    wxString baseURL = "file://" + wxFileName(m_filePath).GetPath(wxPATH_GET_SEPARATOR);
    Logger::get().log("SetPage baseURL: " + baseURL.ToStdString());
    m_webView->SetPage(wxString::FromUTF8(html), baseURL);
    SetStatusText("Rendered: " + m_filePath);
    Logger::get().log("SetPage done");
}

// ---------------------------------------------------------------------------
// HTML escaping
// ---------------------------------------------------------------------------
std::string MDViewerFrame::EscapeHTML(const std::string& text) {
    std::string r;
    r.reserve(text.size() + 16);
    for (unsigned char c : text) {
        switch (c) {
            case '&':  r += "&amp;";  break;
            case '<':  r += "&lt;";   break;
            case '>':  r += "&gt;";   break;
            case '"':  r += "&quot;"; break;
            default:   r += static_cast<char>(c);
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// Inline markdown → HTML  (bold, italic, code, links, images, escape)
// Processes text that is NOT inside a fenced code block.
// ---------------------------------------------------------------------------
std::string MDViewerFrame::ProcessInline(const std::string& text) {
    std::string r;
    r.reserve(text.size() * 2);
    size_t i = 0;
    const size_t n = text.size();

    while (i < n) {
        char c = text[i];

        // Inline code  `…`
        if (c == '`') {
            size_t end = text.find('`', i + 1);
            if (end != std::string::npos) {
                r += "<code>" + EscapeHTML(text.substr(i + 1, end - i - 1)) + "</code>";
                i = end + 1;
                continue;
            }
        }

        // Bold+italic  ***…***
        if (c == '*' && i + 2 < n && text[i+1] == '*' && text[i+2] == '*') {
            size_t end = text.find("***", i + 3);
            if (end != std::string::npos) {
                r += "<strong><em>" + ProcessInline(text.substr(i + 3, end - i - 3)) + "</em></strong>";
                i = end + 3;
                continue;
            }
        }

        // Bold  **…**
        if (c == '*' && i + 1 < n && text[i+1] == '*') {
            size_t end = text.find("**", i + 2);
            if (end != std::string::npos) {
                r += "<strong>" + ProcessInline(text.substr(i + 2, end - i - 2)) + "</strong>";
                i = end + 2;
                continue;
            }
        }

        // Bold  __…__
        if (c == '_' && i + 1 < n && text[i+1] == '_') {
            size_t end = text.find("__", i + 2);
            if (end != std::string::npos) {
                r += "<strong>" + ProcessInline(text.substr(i + 2, end - i - 2)) + "</strong>";
                i = end + 2;
                continue;
            }
        }

        // Italic  *…*
        if (c == '*') {
            size_t end = text.find('*', i + 1);
            if (end != std::string::npos) {
                r += "<em>" + ProcessInline(text.substr(i + 1, end - i - 1)) + "</em>";
                i = end + 1;
                continue;
            }
        }

        // Italic  _…_
        if (c == '_') {
            size_t end = text.find('_', i + 1);
            if (end != std::string::npos) {
                r += "<em>" + ProcessInline(text.substr(i + 1, end - i - 1)) + "</em>";
                i = end + 1;
                continue;
            }
        }

        // Strikethrough  ~~…~~
        if (c == '~' && i + 1 < n && text[i+1] == '~') {
            size_t end = text.find("~~", i + 2);
            if (end != std::string::npos) {
                r += "<del>" + ProcessInline(text.substr(i + 2, end - i - 2)) + "</del>";
                i = end + 2;
                continue;
            }
        }

        // Image  ![alt](url)
        if (c == '!' && i + 1 < n && text[i+1] == '[') {
            size_t aS = i + 2;
            size_t aE = text.find(']', aS);
            if (aE != std::string::npos && aE + 1 < n && text[aE+1] == '(') {
                size_t uS = aE + 2;
                size_t uE = text.find(')', uS);
                if (uE != std::string::npos) {
                    std::string alt = text.substr(aS, aE - aS);
                    std::string url = text.substr(uS, uE - uS);
                    r += "<img src=\"" + url + "\" alt=\"" + EscapeHTML(alt) + "\" loading=\"lazy\">";
                    i = uE + 1;
                    continue;
                }
            }
        }

        // Link  [text](url)
        if (c == '[') {
            size_t tS = i + 1;
            size_t tE = text.find(']', tS);
            if (tE != std::string::npos && tE + 1 < n && text[tE+1] == '(') {
                size_t uS = tE + 2;
                size_t uE = text.find(')', uS);
                if (uE != std::string::npos) {
                    std::string ltext = text.substr(tS, tE - tS);
                    std::string url   = text.substr(uS, uE - uS);
                    r += "<a href=\"" + url + "\">" + ProcessInline(ltext) + "</a>";
                    i = uE + 1;
                    continue;
                }
            }
        }

        // Auto-link  <url>
        if (c == '<') {
            size_t end = text.find('>', i + 1);
            if (end != std::string::npos) {
                std::string inner = text.substr(i + 1, end - i - 1);
                if (inner.find("http") == 0 || inner.find("mailto:") == 0) {
                    r += "<a href=\"" + inner + "\">" + inner + "</a>";
                    i = end + 1;
                    continue;
                }
            }
        }

        // Hard line break: two trailing spaces before \n — handled at block level
        // Fall through: HTML-escape the character
        switch (c) {
            case '&': r += "&amp;";  break;
            case '<': r += "&lt;";   break;
            case '>': r += "&gt;";   break;
            default:  r += c;
        }
        i++;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Markdown block-level parser → HTML body string
// ---------------------------------------------------------------------------
std::string MDViewerFrame::RenderMarkdown(const std::string& md) {
    // Split into lines, stripping \r
    std::vector<std::string> lines;
    {
        std::istringstream ss(md);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }
    }

    // Helpers to classify fences
    auto getFenceInfo = [](const std::string& line) -> std::pair<bool, std::string> {
        if (line.size() < 3) return {false, ""};
        char fc = line[0];
        if (fc != '`' && fc != '~') return {false, ""};
        size_t cnt = 0;
        while (cnt < line.size() && line[cnt] == fc) cnt++;
        if (cnt < 3) return {false, ""};
        std::string lang = line.substr(cnt);
        while (!lang.empty() && (lang.front() == ' ' || lang.front() == '\t')) lang.erase(lang.begin());
        while (!lang.empty() && (lang.back() == ' ' || lang.back() == '\r')) lang.pop_back();
        return {true, lang};
    };

    auto isClosingFence = [](const std::string& line, char fenceChar) -> bool {
        if (line.size() < 3) return false;
        for (char c : line)
            if (c != fenceChar && c != ' ' && c != '\t') return false;
        size_t cnt = 0;
        for (char c : line) if (c == fenceChar) cnt++;
        return cnt >= 3;
    };

    std::string html;
    html.reserve(md.size() * 3);

    // State
    bool inCode    = false;
    bool inMermaid = false;
    char fenceChar = '`';
    std::string mermaidBuf;

    bool inUL = false;
    bool inOL = false;
    bool inBQ = false;
    bool inTable = false;

    std::vector<std::string> paraLines;

    auto flushParagraph = [&]() {
        if (paraLines.empty()) return;
        // Join lines, treating two trailing spaces as <br>
        std::string para;
        for (size_t k = 0; k < paraLines.size(); k++) {
            std::string ln = paraLines[k];
            bool hardBreak = ln.size() >= 2 && ln[ln.size()-1] == ' ' && ln[ln.size()-2] == ' ';
            if (hardBreak) ln.resize(ln.size() - 2);
            if (k > 0) para += hardBreak ? "<br>" : " ";
            para += ln;
        }
        html += "<p>" + ProcessInline(para) + "</p>\n";
        paraLines.clear();
    };

    auto closeLists = [&]() {
        if (inUL) { html += "</ul>\n"; inUL = false; }
        if (inOL) { html += "</ol>\n"; inOL = false; }
    };

    auto closeBlockquote = [&]() {
        if (inBQ) { html += "</blockquote>\n"; inBQ = false; }
    };

    auto closeTable = [&]() {
        if (inTable) { html += "</tbody></table>\n"; inTable = false; }
    };

    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& raw = lines[i];

        // ── Inside fenced code or mermaid block ───────────────────────────
        if (inCode || inMermaid) {
            if (isClosingFence(raw, fenceChar)) {
                if (inMermaid) {
                    html += "<div class=\"mermaid-wrapper\">"
                            "<div class=\"mermaid\">\n" + mermaidBuf + "</div></div>\n";
                    mermaidBuf.clear();
                    inMermaid = false;
                } else {
                    html += "</code></pre>\n";
                    inCode = false;
                }
            } else {
                if (inMermaid)
                    mermaidBuf += raw + "\n";
                else
                    html += EscapeHTML(raw) + "\n";
            }
            continue;
        }

        // ── Opening code fence ────────────────────────────────────────────
        auto [isFence, lang] = getFenceInfo(raw);
        if (isFence) {
            flushParagraph();
            closeLists();
            closeBlockquote();
            closeTable();
            fenceChar = raw[0];
            if (lang == "mermaid") {
                inMermaid = true;
            } else {
                inCode = true;
                if (!lang.empty())
                    html += "<pre><code class=\"language-" + lang + "\">";
                else
                    html += "<pre><code>";
            }
            continue;
        }

        // ── Empty line ─────────────────────────────────────────────────────
        if (raw.empty()) {
            flushParagraph();
            closeLists();
            closeBlockquote();
            closeTable();
            continue;
        }

        // ── Setext headers (look-ahead to next line) ───────────────────────
        if (!raw.empty() && i + 1 < lines.size()) {
            const std::string& nxt = lines[i + 1];
            bool allEq = !nxt.empty() && nxt.find_first_not_of("= \t") == std::string::npos
                         && nxt.find('=') != std::string::npos;
            bool allDash = nxt.size() >= 3 && nxt.find_first_not_of("- \t") == std::string::npos
                           && nxt.find('-') != std::string::npos;
            if (allEq || allDash) {
                flushParagraph();
                closeLists();
                closeBlockquote();
                closeTable();
                int lvl = allEq ? 1 : 2;
                html += "<h" + std::to_string(lvl) + ">"
                      + ProcessInline(raw)
                      + "</h" + std::to_string(lvl) + ">\n";
                i++; // consume underline
                continue;
            }
        }

        // ── ATX headers  # … ######  ──────────────────────────────────────
        if (raw[0] == '#') {
            int lvl = 0;
            while (lvl < 6 && lvl < (int)raw.size() && raw[lvl] == '#') lvl++;
            if (lvl < (int)raw.size() && raw[lvl] == ' ') {
                flushParagraph();
                closeLists();
                closeBlockquote();
                closeTable();
                std::string heading = raw.substr(lvl + 1);
                // Strip optional trailing #s
                {
                    size_t p = heading.find_last_not_of("# ");
                    if (p != std::string::npos) heading = heading.substr(0, p + 1);
                }
                html += "<h" + std::to_string(lvl) + ">"
                      + ProcessInline(heading)
                      + "</h" + std::to_string(lvl) + ">\n";
                continue;
            }
        }

        // ── Horizontal rule  ---  ***  ___  ──────────────────────────────
        {
            std::string stripped;
            for (char c : raw) if (c != ' ' && c != '\t') stripped += c;
            if (stripped.size() >= 3 && paraLines.empty() &&
                (stripped.find_first_not_of('-') == std::string::npos ||
                 stripped.find_first_not_of('*') == std::string::npos ||
                 stripped.find_first_not_of('_') == std::string::npos)) {
                closeLists();
                closeBlockquote();
                closeTable();
                html += "<hr>\n";
                continue;
            }
        }

        // ── Blockquote  >  ────────────────────────────────────────────────
        if (raw.size() >= 2 && raw[0] == '>' && (raw[1] == ' ' || raw[1] == '\t')) {
            flushParagraph();
            closeLists();
            closeTable();
            if (!inBQ) { html += "<blockquote>\n"; inBQ = true; }
            html += "<p>" + ProcessInline(raw.substr(2)) + "</p>\n";
            continue;
        }
        if (inBQ && !raw.empty()) {
            // continuation without leading >
            closeBlockquote();
        }

        // ── Unordered list  -/*/+  ────────────────────────────────────────
        if (raw.size() >= 2 && (raw[0] == '-' || raw[0] == '*' || raw[0] == '+') && raw[1] == ' ') {
            flushParagraph();
            closeBlockquote();
            closeTable();
            if (inOL) { html += "</ol>\n"; inOL = false; }
            if (!inUL) { html += "<ul>\n"; inUL = true; }
            html += "<li>" + ProcessInline(raw.substr(2)) + "</li>\n";
            continue;
        }

        // ── Ordered list  N.  ─────────────────────────────────────────────
        {
            size_t j = 0;
            while (j < raw.size() && std::isdigit((unsigned char)raw[j])) j++;
            if (j > 0 && j < raw.size() - 1 && raw[j] == '.' && raw[j+1] == ' ') {
                flushParagraph();
                closeBlockquote();
                closeTable();
                if (inUL) { html += "</ul>\n"; inUL = false; }
                if (!inOL) { html += "<ol>\n"; inOL = true; }
                html += "<li>" + ProcessInline(raw.substr(j + 2)) + "</li>\n";
                continue;
            }
        }

        // ── GFM table (header row contains |…|…|)  ───────────────────────
        if (raw.find('|') != std::string::npos) {
            // Check if the next line is a separator row (---|---|)
            bool isTblHeader = false;
            if (!inTable && i + 1 < lines.size()) {
                const std::string& sep = lines[i + 1];
                std::string stripped;
                for (char c : sep) if (c != ' ' && c != '\t') stripped += c;
                if (!stripped.empty() && stripped.find_first_not_of("-|:") == std::string::npos
                    && stripped.find('|') != std::string::npos) {
                    isTblHeader = true;
                }
            }
            if (isTblHeader) {
                flushParagraph();
                closeLists();
                closeBlockquote();
                closeTable();
                // Parse header cells
                auto splitCells = [](const std::string& row) -> std::vector<std::string> {
                    std::vector<std::string> cells;
                    std::istringstream ss(row);
                    std::string token;
                    while (std::getline(ss, token, '|')) {
                        while (!token.empty() && token.front() == ' ') token.erase(token.begin());
                        while (!token.empty() && token.back() == ' ') token.pop_back();
                        if (!token.empty()) cells.push_back(token);
                    }
                    return cells;
                };
                html += "<table>\n<thead><tr>\n";
                for (auto& cell : splitCells(raw))
                    html += "  <th>" + ProcessInline(cell) + "</th>\n";
                html += "</tr></thead>\n<tbody>\n";
                inTable = true;
                i++; // skip separator row
                continue;
            }
            if (inTable) {
                auto splitCells = [](const std::string& row) -> std::vector<std::string> {
                    std::vector<std::string> cells;
                    std::istringstream ss(row);
                    std::string token;
                    while (std::getline(ss, token, '|')) {
                        while (!token.empty() && token.front() == ' ') token.erase(token.begin());
                        while (!token.empty() && token.back() == ' ') token.pop_back();
                        if (!token.empty()) cells.push_back(token);
                    }
                    return cells;
                };
                html += "<tr>\n";
                for (auto& cell : splitCells(raw))
                    html += "  <td>" + ProcessInline(cell) + "</td>\n";
                html += "</tr>\n";
                continue;
            }
        }

        // ── Close list if next line is plain text ─────────────────────────
        if (inUL || inOL) {
            closeLists();
        }

        // ── Plain paragraph line ──────────────────────────────────────────
        paraLines.push_back(raw);
    }

    // Flush remaining state
    flushParagraph();
    closeLists();
    closeBlockquote();
    closeTable();
    if (inCode)    html += "</code></pre>\n";
    if (inMermaid) html += "<div class=\"mermaid-wrapper\"><div class=\"mermaid\">\n"
                           + mermaidBuf + "</div></div>\n";

    return html;
}

// ---------------------------------------------------------------------------
// Full HTML page template — CSS variables handle light/dark theming.
// Mermaid theme and zoom SVG background adapt to the active mode.
// ---------------------------------------------------------------------------
std::string MDViewerFrame::WrapWithTemplate(const std::string& body,
                                            const std::string& title,
                                            bool darkMode) {
    const std::string htmlClass    = darkMode ? " class=\"dark\"" : "";
    const std::string mermaidTheme = darkMode ? "dark" : "default";

    return R"HTML(<!DOCTYPE html>
<html lang="en")HTML" + htmlClass + R"HTML(>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>)HTML" + EscapeHTML(title) + R"HTML(</title>
<script>)HTML" + GetMermaidJS() + R"HTML(</script>
<script>)HTML" + GetHighlightJS() + R"HTML(</script>
<style>)HTML" + (darkMode ? GetHighlightCSSDark() : GetHighlightCSSLight()) + R"HTML(</style>
<style>
/* ── Theme tokens ───────────────────────────────────────────────────────── */
:root {
  --bg:            #ffffff;
  --surface:       #f6f8fa;
  --surface2:      #fafbfc;
  --text:          #24292f;
  --text-muted:    #57606a;
  --border:        #d0d7de;
  --link:          #0969da;
  --link-hover:    #0550ae;
  --code-inline:   rgba(175,184,193,0.2);
  --del:           #656d76;
  --zm-svg-bg:     #ffffff;
  --mermaid-hover: rgba(9,105,218,0.16);
  --mermaid-ring:  rgba(9,105,218,0.53);
}
.dark {
  --bg:            #0d1117;
  --surface:       #161b22;
  --surface2:      #1c2128;
  --text:          #e6edf3;
  --text-muted:    #8b949e;
  --border:        #30363d;
  --link:          #58a6ff;
  --link-hover:    #79c0ff;
  --code-inline:   rgba(110,118,129,0.4);
  --del:           #8b949e;
  --zm-svg-bg:     #1e2430;
  --mermaid-hover: rgba(88,166,255,0.12);
  --mermaid-ring:  rgba(88,166,255,0.45);
}

/* ── Reset & base ───────────────────────────────────────────────────────── */
*{box-sizing:border-box;margin:0;padding:0}
body{
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;
  font-size:16px;line-height:1.65;
  color:var(--text);background:var(--bg);
  padding:32px 24px;max-width:960px;margin:0 auto;
}

/* ── Typography ─────────────────────────────────────────────────────────── */
h1,h2,h3,h4,h5,h6{margin:24px 0 16px;font-weight:600;line-height:1.25;color:var(--text)}
h1{font-size:2em;  border-bottom:1px solid var(--border);padding-bottom:.3em}
h2{font-size:1.5em;border-bottom:1px solid var(--border);padding-bottom:.3em}
h3{font-size:1.25em}
h4{font-size:1em}
p{margin-bottom:16px}
a{color:var(--link);text-decoration:none}
a:hover{color:var(--link-hover);text-decoration:underline}
strong{font-weight:600}
del{color:var(--del)}

/* ── Code ───────────────────────────────────────────────────────────────── */
code{
  font-family:'SFMono-Regular',Consolas,'Liberation Mono',Menlo,monospace;
  font-size:85%;background:var(--code-inline);
  padding:.2em .4em;border-radius:3px;color:var(--text);
}
pre{
  background:var(--surface);border:1px solid var(--border);
  border-radius:6px;padding:16px;overflow-x:auto;margin-bottom:16px;
}
pre code{background:none;padding:0;font-size:87.5%;border:none}
.hljs{background:transparent}

/* ── Blockquote ─────────────────────────────────────────────────────────── */
blockquote{
  padding:0 1em;color:var(--text-muted);
  border-left:.25em solid var(--border);margin-bottom:16px;
}

/* ── Lists ──────────────────────────────────────────────────────────────── */
ul,ol{padding-left:2em;margin-bottom:16px}
li{margin:4px 0;color:var(--text)}

/* ── Media ──────────────────────────────────────────────────────────────── */
img{max-width:100%;height:auto;border-radius:4px}

/* ── Rule ───────────────────────────────────────────────────────────────── */
hr{height:.25em;padding:0;margin:24px 0;background:var(--border);border:0}

/* ── Table ──────────────────────────────────────────────────────────────── */
table{border-collapse:collapse;margin-bottom:16px;width:100%}
th,td{border:1px solid var(--border);padding:6px 13px;text-align:left;color:var(--text)}
th{background:var(--surface);font-weight:600}
tr:nth-child(even) td{background:var(--surface)}

/* ── Mermaid wrapper ────────────────────────────────────────────────────── */
.mermaid-wrapper{
  display:inline-block;cursor:zoom-in;
  border:1px solid var(--border);border-radius:6px;
  padding:20px;margin-bottom:16px;
  background:var(--surface2);
  transition:box-shadow .18s,border-color .18s;
  width:100%;text-align:center;
}
.mermaid-wrapper:hover{
  box-shadow:0 0 0 3px var(--mermaid-hover);
  border-color:var(--mermaid-ring);
}

/* ── Zoom modal ─────────────────────────────────────────────────────────── */
#zm-overlay{
  display:none;position:fixed;inset:0;background:rgba(0,0,0,.88);
  z-index:9999;align-items:center;justify-content:center;
}
#zm-overlay.open{display:flex}
#zm-stage{
  position:relative;width:90vw;height:90vh;
  overflow:hidden;cursor:grab;user-select:none;
}
#zm-stage.dragging{cursor:grabbing}
#zm-inner{
  display:flex;align-items:center;justify-content:center;
  width:100%;height:100%;transform-origin:center center;
}
#zm-inner svg{
  max-width:85vw;max-height:85vh;
  background:var(--zm-svg-bg);border-radius:8px;padding:24px;
  box-shadow:0 8px 32px rgba(0,0,0,.5);
}
#zm-close{
  position:fixed;top:14px;right:18px;background:rgba(255,255,255,.12);
  border:none;color:#fff;font-size:20px;width:36px;height:36px;
  border-radius:50%;cursor:pointer;z-index:10001;
  display:flex;align-items:center;justify-content:center;
  transition:background .15s;
}
#zm-close:hover{background:rgba(255,255,255,.28)}
#zm-hint{
  position:fixed;bottom:14px;left:50%;transform:translateX(-50%);
  color:rgba(255,255,255,.55);font-size:12px;pointer-events:none;white-space:nowrap;
}
#zm-scale{
  position:fixed;top:16px;left:50%;transform:translateX(-50%);
  color:rgba(255,255,255,.7);font-size:12px;background:rgba(0,0,0,.4);
  padding:3px 10px;border-radius:20px;pointer-events:none;
}
</style>
</head>
<body>
)HTML" + body + R"HTML(

<!-- ── Zoom modal ───────────────────────────────────────────────────────── -->
<div id="zm-overlay">
  <button id="zm-close" title="Close (Esc)">&#x2715;</button>
  <div id="zm-stage">
    <div id="zm-inner"></div>
  </div>
  <div id="zm-hint">Scroll to zoom &nbsp;·&nbsp; Drag to pan &nbsp;·&nbsp; ESC to close</div>
  <div id="zm-scale">100%</div>
</div>

<script>
// ── Highlight.js ─────────────────────────────────────────────────────────
hljs.highlightAll();

// ── Mermaid init (theme set by C++ based on current mode) ────────────────
mermaid.initialize({startOnLoad:true, theme:')HTML" + mermaidTheme + R"HTML(', securityLevel:'loose'});

// ── Zoom state ───────────────────────────────────────────────────────────
let zmScale = 1, zmTX = 0, zmTY = 0;
let zmDrag = false, zmDX = 0, zmDY = 0;
const zmInner = document.getElementById('zm-inner');
const zmStage = document.getElementById('zm-stage');
const zmScaleLabel = document.getElementById('zm-scale');

function zmApply() {
  zmInner.style.transform = `translate(${zmTX}px,${zmTY}px) scale(${zmScale})`;
  zmScaleLabel.textContent = Math.round(zmScale * 100) + '%';
}

function zmOpen(svgHTML) {
  zmScale=1; zmTX=0; zmTY=0;
  zmInner.innerHTML = svgHTML;
  const svg = zmInner.querySelector('svg');
  if (svg) {
    svg.removeAttribute('width');
    svg.removeAttribute('height');
    svg.style.maxWidth  = '85vw';
    svg.style.maxHeight = '85vh';
  }
  zmApply();
  document.getElementById('zm-overlay').classList.add('open');
  document.body.style.overflow = 'hidden';
}

function zmClose() {
  document.getElementById('zm-overlay').classList.remove('open');
  document.body.style.overflow = '';
}

// Click on any mermaid wrapper → zoom (event delegation, works after async render)
document.addEventListener('click', e => {
  if (e.target.closest('#zm-overlay')) return;
  const w = e.target.closest('.mermaid-wrapper');
  if (w) { const svg = w.querySelector('svg'); if (svg) zmOpen(svg.outerHTML); }
});

document.getElementById('zm-close').addEventListener('click', zmClose);
document.getElementById('zm-overlay').addEventListener('click', e => {
  if (e.target === document.getElementById('zm-overlay')) zmClose();
});

document.addEventListener('keydown', e => { if (e.key === 'Escape') zmClose(); });

zmStage.addEventListener('wheel', e => {
  e.preventDefault();
  zmScale = Math.min(Math.max(zmScale * (e.deltaY < 0 ? 1.12 : 0.9), 0.08), 20);
  zmApply();
}, {passive:false});

zmStage.addEventListener('mousedown', e => {
  if (e.button !== 0) return;
  zmDrag = true; zmDX = e.clientX - zmTX; zmDY = e.clientY - zmTY;
  zmStage.classList.add('dragging'); e.preventDefault();
});
document.addEventListener('mousemove', e => {
  if (!zmDrag) return;
  zmTX = e.clientX - zmDX; zmTY = e.clientY - zmDY; zmApply();
});
document.addEventListener('mouseup', () => {
  zmDrag = false; zmStage.classList.remove('dragging');
});

let lastDist = 0;
zmStage.addEventListener('touchstart', e => {
  if (e.touches.length === 1) {
    zmDrag = true; zmDX = e.touches[0].clientX - zmTX; zmDY = e.touches[0].clientY - zmTY;
  } else if (e.touches.length === 2) {
    zmDrag = false;
    lastDist = Math.hypot(e.touches[0].clientX - e.touches[1].clientX,
                          e.touches[0].clientY - e.touches[1].clientY);
  }
  e.preventDefault();
}, {passive:false});
zmStage.addEventListener('touchmove', e => {
  if (e.touches.length === 1 && zmDrag) {
    zmTX = e.touches[0].clientX - zmDX; zmTY = e.touches[0].clientY - zmDY; zmApply();
  } else if (e.touches.length === 2) {
    const d = Math.hypot(e.touches[0].clientX - e.touches[1].clientX,
                         e.touches[0].clientY - e.touches[1].clientY);
    if (lastDist > 0) {
      zmScale = Math.min(Math.max(zmScale * (d / lastDist), 0.08), 20); zmApply();
    }
    lastDist = d;
  }
  e.preventDefault();
}, {passive:false});
zmStage.addEventListener('touchend', () => { zmDrag = false; lastDist = 0; });
</script>
</body>
</html>
)HTML";
}

// ---------------------------------------------------------------------------
// Menu handlers
// ---------------------------------------------------------------------------
void MDViewerFrame::OnViewLogs(wxCommandEvent&) {
    std::string logPath = std::string(getenv("HOME") ?: "") + "/Library/Logs/MDViewer/mdviewer.log";
    std::string raw = ReadFile(logPath);

    const std::string bg      = m_darkMode ? "#0d1117" : "#ffffff";
    const std::string surface = m_darkMode ? "#161b22" : "#f6f8fa";
    const std::string border  = m_darkMode ? "#30363d" : "#d0d7de";
    const std::string text    = m_darkMode ? "#e6edf3" : "#24292f";
    const std::string muted   = m_darkMode ? "#8b949e" : "#57606a";
    const std::string green   = m_darkMode ? "#3fb950" : "#1a7f37";
    const std::string red     = m_darkMode ? "#f85149" : "#cf222e";

    std::string rows;
    std::istringstream ss(raw);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        // Split "YYYY-MM-DD HH:MM:SS  <message>"
        std::string ts, msg;
        if (line.size() > 21 && line[10] == ' ' && line[19] == ' ') {
            ts  = line.substr(0, 19);
            msg = line.substr(21);  // skip two-space separator
        } else {
            msg = line;
        }
        // Colour coding: errors red, startup entries green
        std::string msgColor = text;
        if (msg.find("FAILED") != std::string::npos || msg.find("error") != std::string::npos)
            msgColor = red;
        else if (msg.find("=== startup") != std::string::npos)
            msgColor = green;

        rows += "<tr>"
                "<td class='ts'>" + EscapeHTML(ts) + "</td>"
                "<td style='color:" + msgColor + "'>" + EscapeHTML(msg) + "</td>"
                "</tr>\n";
    }

    std::string html = R"HTML(<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<title>MDViewer — Logs</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'SFMono-Regular',Consolas,monospace;font-size:13px;
     background:)HTML" + bg + R"HTML(;color:)HTML" + text + R"HTML(;padding:24px}
h2{font-size:15px;font-weight:600;margin-bottom:16px;color:)HTML" + text + R"HTML(}
table{width:100%;border-collapse:collapse}
tr{border-bottom:1px solid )HTML" + border + R"HTML(}
tr:last-child{border-bottom:none}
td{padding:5px 10px;vertical-align:top;white-space:pre-wrap;word-break:break-all}
.ts{color:)HTML" + muted + R"HTML(;white-space:nowrap;padding-right:20px;user-select:none}
tr:hover{background:)HTML" + surface + R"HTML(}
</style></head><body>
<h2>MDViewer — Application Log</h2>
<p style="font-size:12px;color:)HTML" + muted + R"HTML(;margin:8px 0 16px">)HTML"
+ EscapeHTML(logPath) + R"HTML(</p>
<table>)HTML" + rows + R"HTML(</table>
</body></html>)HTML";

    m_webView->SetPage(wxString::FromUTF8(html), "");
    SetStatusText("Viewing logs — use View > View Document to return");
}

void MDViewerFrame::OnViewDoc(wxCommandEvent&) {
    LoadAndRender();
}

void MDViewerFrame::OnThemeLight(wxCommandEvent&) {
    if (m_darkMode) {
        m_darkMode = false;
        wxConfig cfg("MDViewer");
        cfg.Write("darkMode", false);
        LoadAndRender();
    }
}

void MDViewerFrame::OnThemeDark(wxCommandEvent&) {
    if (!m_darkMode) {
        m_darkMode = true;
        wxConfig cfg("MDViewer");
        cfg.Write("darkMode", true);
        LoadAndRender();
    }
}

void MDViewerFrame::OnOpen(wxCommandEvent&) {
    wxFileDialog dlg(this, "Open file", "", "",
                     "Markdown and HTML files (*.md;*.html;*.htm)|*.md;*.html;*.htm"
                     "|Markdown files (*.md)|*.md"
                     "|HTML files (*.html;*.htm)|*.html;*.htm"
                     "|All files (*)|*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL) return;
    m_filePath = dlg.GetPath();
    SetTitle("MDViewer — " + wxFileName(m_filePath).GetFullName());
    LoadAndRender();
}

void MDViewerFrame::OnReload(wxCommandEvent&) {
    LoadAndRender();
}

void MDViewerFrame::OnExit(wxCommandEvent&) {
    Close(true);
}

void MDViewerFrame::OnClose(wxCloseEvent& evt) {
    Destroy();
    evt.Skip();
}
