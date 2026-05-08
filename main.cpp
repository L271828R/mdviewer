#include <wx/wx.h>
#include "mdviewer.h"

class MDViewerApp : public wxApp {
public:
    bool OnInit() override;
};

wxIMPLEMENT_APP(MDViewerApp);

bool MDViewerApp::OnInit() {
    if (argc < 2) {
        wxMessageBox(
            "Usage: mdviewer <file.md|file.html>",
            "MDViewer", wxOK | wxICON_INFORMATION);
        return false;
    }

    wxString path = argv[1];
    if (!wxFileExists(path)) {
        wxMessageBox("File not found: " + path, "MDViewer", wxOK | wxICON_ERROR);
        return false;
    }

    MDViewerFrame* frame = new MDViewerFrame(path);
    frame->Show(true);
    return true;
}
