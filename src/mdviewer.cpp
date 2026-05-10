#include "mdviewer.h"
#include "markdown.h"
#include "html_template.h"
#include <wx/webview.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/config.h>
#include <fstream>
#include <sstream>

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
    EVT_MENU(ID_FONT_INCREASE, MDViewerFrame::OnFontIncrease)
    EVT_MENU(ID_FONT_DECREASE, MDViewerFrame::OnFontDecrease)
    EVT_MENU(ID_FONT_RESET,    MDViewerFrame::OnFontReset)
    EVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED(wxID_ANY, MDViewerFrame::OnScriptMessage)
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
    , m_fontSizePercent(100)
{
    wxFileName fn(filePath);
    fn.MakeAbsolute();
    m_filePath = fn.GetFullPath();

    wxConfig cfg("MDViewer");
    m_darkMode        = cfg.ReadBool("darkMode", false);
    m_fontSizePercent = (int)cfg.ReadLong("fontSizePercent", 100);

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
    view->AppendSeparator();
    view->Append(ID_FONT_INCREASE, "Increase Font Size\tCtrl++");
    view->Append(ID_FONT_DECREASE, "Decrease Font Size\tCtrl+-");
    view->Append(ID_FONT_RESET,    "Reset Font Size\tCtrl+0");
    bar->Append(view, "&View");

    SetMenuBar(bar);

    CreateStatusBar();
    SetStatusText("Loading…");

    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("fontSizeChange");
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
        m_webView->LoadURL(url);
        SetStatusText("Loaded HTML: " + m_filePath);
        return;
    }

    std::string raw  = ReadFile(m_filePath.ToStdString());
    std::string body = RenderMarkdown(raw);
    std::string title = wxFileName(m_filePath).GetFullName().ToStdString();
    std::string html  = BuildHTML(body, title, m_darkMode, m_fontSizePercent);

    wxString baseURL = "file://" + wxFileName(m_filePath).GetPath(wxPATH_GET_SEPARATOR);
    m_webView->SetPage(wxString::FromUTF8(html), baseURL);
    SetStatusText("Rendered: " + m_filePath);
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
        std::string ts, msg;
        if (line.size() > 21 && line[10] == ' ' && line[19] == ' ') {
            ts  = line.substr(0, 19);
            msg = line.substr(21);
        } else {
            msg = line;
        }
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

void MDViewerFrame::OnViewDoc(wxCommandEvent&)  { LoadAndRender(); }

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

void MDViewerFrame::OnReload(wxCommandEvent&) { LoadAndRender(); }
void MDViewerFrame::OnExit(wxCommandEvent&)   { Close(true); }

void MDViewerFrame::OnClose(wxCloseEvent& evt) {
    Destroy();
    evt.Skip();
}

void MDViewerFrame::OnFontIncrease(wxCommandEvent&) {
    m_fontSizePercent = std::min(200, m_fontSizePercent + 10);
    wxConfig cfg("MDViewer");
    cfg.Write("fontSizePercent", (long)m_fontSizePercent);
    LoadAndRender();
}

void MDViewerFrame::OnFontDecrease(wxCommandEvent&) {
    m_fontSizePercent = std::max(50, m_fontSizePercent - 10);
    wxConfig cfg("MDViewer");
    cfg.Write("fontSizePercent", (long)m_fontSizePercent);
    LoadAndRender();
}

void MDViewerFrame::OnFontReset(wxCommandEvent&) {
    m_fontSizePercent = 100;
    wxConfig cfg("MDViewer");
    cfg.Write("fontSizePercent", 100L);
    LoadAndRender();
}

void MDViewerFrame::OnScriptMessage(wxWebViewEvent& evt) {
    long val;
    if (evt.GetURL().ToLong(&val)) {
        m_fontSizePercent = (int)std::max(50L, std::min(200L, val));
        wxConfig cfg("MDViewer");
        cfg.Write("fontSizePercent", (long)m_fontSizePercent);
    }
}
