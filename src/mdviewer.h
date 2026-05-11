#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <wx/filename.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <string>
#include "logger.h"

enum {
    ID_RELOAD       = wxID_HIGHEST + 1,
    ID_THEME_LIGHT,
    ID_THEME_DARK,
    ID_VIEW_LOGS,
    ID_VIEW_DOC,
    ID_FONT_INCREASE,
    ID_FONT_DECREASE,
    ID_FONT_RESET,
    ID_FIND_NEXT,
    ID_FIND_PREV,
    ID_FIND_CLOSE,
};

class MDViewerFrame : public wxFrame {
public:
    explicit MDViewerFrame(const wxString& filePath);

private:
    wxWebView*    m_webView;
    wxString      m_filePath;
    bool          m_darkMode;
    int           m_fontSizePercent;
    wxPanel*      m_findPanel   = nullptr;
    wxTextCtrl*   m_findCtrl    = nullptr;
    wxStaticText* m_findStatus  = nullptr;
    wxString      m_findTerm;
    int           m_findTotal   = 0;
    int           m_findCurrent = 0;

    void LoadAndRender();
    std::string ReadFile(const std::string& path);
    void ShowFindBar(bool show);
    void PositionFindBar();
    void DoFind(bool forward);

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
    void OnCopy(wxCommandEvent& evt);
    void OnSelectAll(wxCommandEvent& evt);
    void OnPasteView(wxCommandEvent& evt);
    void OnFindOpen(wxCommandEvent& evt);
    void OnFindNext(wxCommandEvent& evt);
    void OnFindPrev(wxCommandEvent& evt);
    void OnFindClose(wxCommandEvent& evt);

    wxDECLARE_EVENT_TABLE();
};
