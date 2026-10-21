#include "FileDialog.hpp"

#include <shobjidl.h>
#include <wrl/client.h>

namespace Spool {

    std::wstring pickMapFile(HWND owner) {
        using Microsoft::WRL::ComPtr;

        ComPtr<IFileOpenDialog> dialog;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog))))
            return L"";

        COMDLG_FILTERSPEC filters[] = {
            { L"Halo cache files (*.map)", L"*.map" },
            { L"All files (*.*)", L"*.*" },
        };
        dialog->SetFileTypes((UINT)std::size(filters), filters);
        dialog->SetDefaultExtension(L"map");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);

        if (FAILED(dialog->Show(owner)))
            return L"";

        ComPtr<IShellItem> item;
        if (FAILED(dialog->GetResult(&item)))
            return L"";

        PWSTR raw = nullptr;
        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)))
            return L"";

        std::wstring path = raw;
        CoTaskMemFree(raw);
        return path;
    }

}
