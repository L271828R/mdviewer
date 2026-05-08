#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <wx/filename.h>
#include <string>

enum {
    ID_RELOAD       = wxID_HIGHEST + 1,
    ID_THEME_LIGHT,
    ID_THEME_DARK,
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
    void OnExit(wxCommandEvent& evt);
    void OnClose(wxCloseEvent& evt);

    wxDECLARE_EVENT_TABLE();
};
