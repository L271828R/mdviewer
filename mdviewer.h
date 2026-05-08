#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <wx/filename.h>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

// ---------------------------------------------------------------------------
// Logger — appends timestamped lines to ~/Library/Logs/MDViewer/mdviewer.log
// ---------------------------------------------------------------------------
class Logger {
public:
    static Logger& get() {
        static Logger instance;
        return instance;
    }

    void log(const std::string& msg) {
        if (!m_file.is_open()) return;
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        std::ostringstream line;
        line << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
             << "  " << msg << "\n";
        m_file << line.str();
        m_file.flush();
    }

private:
    Logger() {
        // macOS conventional log location
        std::string dir = std::string(getenv("HOME") ?: "") + "/Library/Logs/MDViewer";
        ::system(("mkdir -p \"" + dir + "\"").c_str());
        m_file.open(dir + "/mdviewer.log", std::ios::app);
    }
    std::ofstream m_file;
};

enum {
    ID_RELOAD       = wxID_HIGHEST + 1,
    ID_THEME_LIGHT,
    ID_THEME_DARK,
    ID_VIEW_LOGS,
    ID_VIEW_DOC,
};

class MDViewerFrame : public wxFrame {
public:
    explicit MDViewerFrame(const wxString& filePath);

private:
    wxWebView* m_webView;
    wxString   m_filePath;
    bool       m_darkMode;

    void LoadAndRender();
    std::string ReadFile(const std::string& path);

    // Markdown → HTML body
    std::string RenderMarkdown(const std::string& md);

    // Inline formatting: bold, italic, code, links, images
    std::string ProcessInline(const std::string& text);

    // HTML-escape a raw string (no markdown processing)
    static std::string EscapeHTML(const std::string& text);

    // Wrap body in full HTML page with CSS, Mermaid.js and zoom JS
    std::string WrapWithTemplate(const std::string& body,
                                 const std::string& title,
                                 bool darkMode);

    void OnOpen(wxCommandEvent& evt);
    void OnReload(wxCommandEvent& evt);
    void OnThemeLight(wxCommandEvent& evt);
    void OnThemeDark(wxCommandEvent& evt);
    void OnViewLogs(wxCommandEvent& evt);
    void OnViewDoc(wxCommandEvent& evt);
    void OnExit(wxCommandEvent& evt);
    void OnClose(wxCloseEvent& evt);

    wxDECLARE_EVENT_TABLE();
};
