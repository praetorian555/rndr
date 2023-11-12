#include <windows.h>
#include <commdlg.h>

#include "rndr/core/containers/string.h"

Rndr::String OpenFileDialog()
{
    OPENFILENAME ofn;
    char fileName[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;  // If you have a window to center over, put its HANDLE here
    ofn.lpstrFilter = "Any File\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select a File";
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

    GetOpenFileName(&ofn);
    return fileName;
}