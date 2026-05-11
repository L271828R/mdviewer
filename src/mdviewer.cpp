#include "mdviewer.h"
#include "markdown.h"
#include "html_template.h"
#include "inspector.h"
#include <wx/webview.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/config.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <fstream>
#include <sstream>

// ---------------------------------------------------------------------------
// Event table
// ---------------------------------------------------------------------------
wxBEGIN_EVENT_TABLE(MDViewerFrame, wxFrame)
    EVT_MENU(wxID_OPEN,      MDViewerFrame::OnOpen)
    EVT_MENU(ID_RELOAD,      MDViewerFrame::OnReload)
    EVT_MENU(wxID_COPY,      MDViewerFrame::OnCopy)
    EVT_MENU(wxID_SELECTALL, MDViewerFrame::OnSelectAll)
    EVT_MENU(wxID_PASTE,     MDViewerFrame::OnPasteView)
    EVT_MENU(wxID_FIND,      MDViewerFrame::OnFindOpen)
    EVT_MENU(ID_FIND_NEXT,   MDViewerFrame::OnFindNext)
    EVT_MENU(ID_FIND_PREV,   MDViewerFrame::OnFindPrev)
    EVT_MENU(ID_FIND_CLOSE,  MDViewerFrame::OnFindClose)
    EVT_MENU(ID_THEME_LIGHT, MDViewerFrame::OnThemeLight)
    EVT_MENU(ID_THEME_DARK,  MDViewerFrame::OnThemeDark)
    EVT_MENU(ID_VIEW_LOGS,   MDViewerFrame::OnViewLogs)
    EVT_MENU(ID_VIEW_DOC,    MDViewerFrame::OnViewDoc)
    EVT_MENU(ID_FONT_INCREASE, MDViewerFrame::OnFontIncrease)
    EVT_MENU(ID_FONT_DECREASE, MDViewerFrame::OnFontDecrease)
    EVT_MENU(ID_FONT_RESET,    MDViewerFrame::OnFontReset)
    EVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED(wxID_ANY, MDViewerFrame::OnScriptMessage)
    EVT_MENU(wxID_CLOSE,     MDViewerFrame::OnExit)
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
    file->Append(wxID_CLOSE, "&Close Window\tCtrl+W");
    file->Append(wxID_EXIT,  "E&xit\tCtrl+Q");
    bar->Append(file, "&File");

    // ── Edit menu ────────────────────────────────────────────────────────
    wxMenu* edit = new wxMenu();
    edit->Append(wxID_COPY,      "&Copy\tCtrl+C");
    edit->Append(wxID_SELECTALL, "Select &All\tCtrl+A");
    edit->AppendSeparator();
    edit->Append(wxID_PASTE,     "&Paste && Render\tCtrl+V");
    edit->AppendSeparator();
    edit->Append(wxID_FIND,      "&Find…\tCtrl+F");
    edit->Append(ID_FIND_NEXT,   "Find &Next\tCtrl+G");
    edit->Append(ID_FIND_PREV,   "Find &Previous\tCtrl+Shift+G");
    bar->Append(edit, "&Edit");

    // ── View menu ────────────────────────────────────────────────────────
    wxMenu* view = new wxMenu();
    view->AppendRadioItem(ID_THEME_LIGHT, "&Light Mode\tCtrl+Shift+L");
    view->AppendRadioItem(ID_THEME_DARK,  "&Dark Mode\tCtrl+Shift+D");
    view->Check(m_darkMode ? ID_THEME_DARK : ID_THEME_LIGHT, true);
    view->AppendSeparator();
    view->Append(ID_VIEW_LOGS, "View &Logs");
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
    m_webView->AddScriptMessageHandler("clipboardCopy");
    EnableWebInspector(m_webView);

    // ── Find bar — floating overlay, top-right ───────────────────────────
    m_findPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxTAB_TRAVERSAL | wxBORDER_SIMPLE);
    auto* fs = new wxBoxSizer(wxHORIZONTAL);

    m_findCtrl = new wxTextCtrl(m_findPanel, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxSize(180, -1), wxTE_PROCESS_ENTER);
    auto* prevBtn = new wxButton(m_findPanel, ID_FIND_PREV, "‹",
                                 wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    auto* nextBtn = new wxButton(m_findPanel, ID_FIND_NEXT, "›",
                                 wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_findStatus = new wxStaticText(m_findPanel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(72, -1));
    auto* closeBtn = new wxButton(m_findPanel, ID_FIND_CLOSE, "×",
                                  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);

    fs->AddSpacer(6);
    fs->Add(m_findCtrl,   0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->AddSpacer(4);
    fs->Add(prevBtn,      0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->Add(nextBtn,      0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->AddSpacer(6);
    fs->Add(m_findStatus, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->AddSpacer(4);
    fs->Add(closeBtn,     0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->AddSpacer(6);
    m_findPanel->SetSizer(fs);
    m_findPanel->Hide();

    closeBtn->Bind(wxEVT_BUTTON,   [this](wxCommandEvent&) { ShowFindBar(false); });
    nextBtn->Bind(wxEVT_BUTTON,    [this](wxCommandEvent&) { DoFind(true); });
    prevBtn->Bind(wxEVT_BUTTON,    [this](wxCommandEvent&) { DoFind(false); });
    m_findCtrl->Bind(wxEVT_TEXT,   [this](wxCommandEvent&) {
        m_findTerm = wxEmptyString;
        m_webView->Find(wxEmptyString);
        DoFind(true);
    });
    m_findCtrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { DoFind(true); });
    m_findCtrl->Bind(wxEVT_KEY_DOWN,   [this](wxKeyEvent& evt) {
        if (evt.GetKeyCode() == WXK_ESCAPE) ShowFindBar(false);
        else evt.Skip();
    });

    // ── Layout — webview fills frame; find panel floats above it ─────────
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    Bind(wxEVT_SIZE, [this](wxSizeEvent& evt) {
        evt.Skip();
        CallAfter([this]() { PositionFindBar(); });
    });

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
    const std::string logPath = std::string(getenv("HOME") ?: "") + "/Library/Logs/MDViewer/mdviewer.log";
    std::string html = BuildLogsHTML(ReadFile(logPath), logPath, m_darkMode);
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

// ---------------------------------------------------------------------------
// Edit: copy, select-all, paste-to-render
// ---------------------------------------------------------------------------
void MDViewerFrame::OnCopy(wxCommandEvent&)      { m_webView->Copy(); }
void MDViewerFrame::OnSelectAll(wxCommandEvent&) { m_webView->SelectAll(); }

void MDViewerFrame::OnPasteView(wxCommandEvent&) {
    if (!wxTheClipboard->Open()) return;
    wxTextDataObject data;
    bool ok = wxTheClipboard->IsSupported(wxDF_TEXT) && wxTheClipboard->GetData(data);
    wxTheClipboard->Close();
    if (!ok) return;

    std::string md   = data.GetText().ToStdString();
    std::string body = RenderMarkdown(md);
    std::string html = BuildHTML(body, "Clipboard", m_darkMode, m_fontSizePercent);
    m_webView->SetPage(wxString::FromUTF8(html), wxEmptyString);
    wxString status = "Rendering clipboard markdown";
    if (!m_filePath.empty()) status += "  (Ctrl+R to return to file)";
    SetStatusText(status);
}

// ---------------------------------------------------------------------------
// Find bar
// ---------------------------------------------------------------------------
void MDViewerFrame::OnFindOpen(wxCommandEvent&) {
    if (m_findPanel->IsShown()) {
        m_findCtrl->SetFocus();
        m_findCtrl->SelectAll();
    } else {
        ShowFindBar(true);
    }
}

void MDViewerFrame::OnFindNext(wxCommandEvent&)  { DoFind(true); }
void MDViewerFrame::OnFindPrev(wxCommandEvent&)  { DoFind(false); }
void MDViewerFrame::OnFindClose(wxCommandEvent&) { ShowFindBar(false); }

void MDViewerFrame::ShowFindBar(bool show) {
    if (show) {
        m_findPanel->Show();
        PositionFindBar();
        m_findCtrl->SetFocus();
        m_findCtrl->SelectAll();
    } else {
        m_findPanel->Hide();
        m_webView->Find(wxEmptyString);
        m_findStatus->SetLabel(wxEmptyString);
        m_findTotal = 0; m_findCurrent = 0; m_findTerm.clear();
        m_webView->SetFocus();
    }
}

void MDViewerFrame::PositionFindBar() {
    if (!m_findPanel || !m_findPanel->IsShown()) return;
    wxSize panel = m_findPanel->GetBestSize();
    wxSize client = GetClientSize();
    m_findPanel->SetSize(client.x - panel.x - 12, 8, panel.x, panel.y);
    m_findPanel->Raise();
}

void MDViewerFrame::DoFind(bool forward) {
    wxString term = m_findCtrl->GetValue();
    if (term.empty()) {
        m_webView->Find(wxEmptyString);
        m_findStatus->SetLabel(wxEmptyString);
        m_findTotal = 0; m_findCurrent = 0; m_findTerm.clear();
        return;
    }

    bool newSearch = (term != m_findTerm);
    m_findTerm = term;

    // wxWebView::Find() returns -1 on macOS WKWebView (async under the hood),
    // so count via JS instead — only when the term changes.
    if (newSearch) {
        wxString escaped = term.Lower();
        escaped.Replace("\\", "\\\\");
        escaped.Replace("\"", "\\\"");
        wxString js = wxString::Format(
            "(document.body.innerText.toLowerCase().split(\"%s\").length - 1)",
            escaped);
        wxString result;
        long n = 0;
        m_findTotal = (m_webView->RunScript(js, &result) && result.ToLong(&n))
                      ? (int)std::max(0L, n) : 0;
    }

    int flags = wxWEBVIEW_FIND_HIGHLIGHT_RESULT | wxWEBVIEW_FIND_WRAP;
    if (!forward) flags |= wxWEBVIEW_FIND_BACKWARDS;
    m_webView->Find(term, static_cast<wxWebViewFindFlags>(flags));

    if (m_findTotal == 0) {
        m_findCurrent = 0;
        m_findStatus->SetLabel("No results");
    } else {
        if (newSearch) {
            m_findCurrent = 1;
        } else if (forward) {
            m_findCurrent = m_findCurrent % m_findTotal + 1;
        } else {
            m_findCurrent = (m_findCurrent - 2 + m_findTotal) % m_findTotal + 1;
        }
        m_findStatus->SetLabel(wxString::Format("%d of %d", m_findCurrent, m_findTotal));
    }
}

// ---------------------------------------------------------------------------
void MDViewerFrame::OnScriptMessage(wxWebViewEvent& evt) {
    if (evt.GetMessageHandler() == "clipboardCopy") {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(evt.GetString()));
            wxTheClipboard->Close();
        }
        return;
    }
    long val;
    if (evt.GetString().ToLong(&val)) {
        m_fontSizePercent = (int)std::max(50L, std::min(200L, val));
        wxConfig cfg("MDViewer");
        cfg.Write("fontSizePercent", (long)m_fontSizePercent);
    }
}
