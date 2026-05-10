#include <wx/wx.h>
#include "mdviewer.h"
#include "markdown.h"
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <cstdio>
#include <cstring>

extern char **environ;

class MDViewerApp : public wxApp {
public:
    bool OnInit() override;
};

wxIMPLEMENT_APP_NO_MAIN(MDViewerApp);

int main(int argc, char* argv[]) {
    if (argc >= 2 && std::strcmp(argv[1], "--llm") == 0) {
        std::fputs(GetLLMReadme().c_str(), stdout);
        return 0;
    }

    // When launched from a terminal, re-exec self as a detached process.
    // posix_spawn (not fork) gives the child fresh Mach/XPC state so
    // WKWebView's rendering pipeline initialises correctly.
    if (isatty(STDIN_FILENO)) {
        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        posix_spawn_file_actions_addopen(&fa, STDIN_FILENO,  "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        pid_t child;
        posix_spawnp(&child, argv[0], &fa, nullptr, argv, environ);
        posix_spawn_file_actions_destroy(&fa);
        return 0;
    }

    char cwdbuf[4096] = {};
    std::string cwd = getcwd(cwdbuf, sizeof(cwdbuf)) ? cwdbuf : "?";
    Logger::get().log("=== startup  pid=" + std::to_string(getpid())
                      + "  arg=" + (argc >= 2 ? argv[1] : "(none)")
                      + "  cwd=" + cwd);

    return wxEntry(argc, argv);
}

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
