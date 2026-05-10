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
    ID_FONT_INCREASE,
    ID_FONT_DECREASE,
    ID_FONT_RESET,
};

class MDViewerFrame : public wxFrame {
public:
    explicit MDViewerFrame(const wxString& filePath);

private:
    wxWebView* m_webView;
    wxString   m_filePath;
    bool       m_darkMode;
    int        m_fontSizePercent;

    void LoadAndRender();
    std::string ReadFile(const std::string& path);

    void OnOpen(wxCommandEvent& evt);
    void OnReload(wxCommandEvent& evt);
    void OnThemeLight(wxCommandEvent& evt);
    void OnThemeDark(wxCommandEvent& evt);
    void OnViewLogs(wxCommandEvent& evt);
    void OnViewDoc(wxCommandEvent& evt);
    void OnFontIncrease(wxCommandEvent& evt);
    void OnFontDecrease(wxCommandEvent& evt);
    void OnFontReset(wxCommandEvent& evt);
    void OnScriptMessage(wxWebViewEvent& evt);
    void OnExit(wxCommandEvent& evt);
    void OnClose(wxCloseEvent& evt);

    wxDECLARE_EVENT_TABLE();
};
