#include "FileDialog.hpp"

#include <shobjidl.h>
#include <wrl/client.h>
#include <system_error>

namespace Spool {
    namespace {
        using Microsoft::WRL::ComPtr;

        void check(HRESULT hr, const char* operation) {
            if (FAILED(hr)) throw std::system_error(static_cast<int>(hr), std::system_category(), operation);
        }

        std::wstring show(IFileDialog* dialog, HWND owner, DWORD flags) {
            COMDLG_FILTERSPEC filters[] = {
                { L"Halo cache files (*.map)", L"*.map" },
                { L"All files (*.*)", L"*.*" },
            };
            check(dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters), "Cannot set map filters");
            check(dialog->SetDefaultExtension(L"map"), "Cannot set map extension");
            DWORD options = 0;
            check(dialog->GetOptions(&options), "Cannot read dialog options");
            check(dialog->SetOptions(options | flags | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR), "Cannot set dialog options");
            auto hr = dialog->Show(owner);
            if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
            check(hr, "Cannot show map dialog");
            ComPtr<IShellItem> item;
            check(dialog->GetResult(&item), "Cannot get selected map");
            PWSTR raw = nullptr;
            check(item->GetDisplayName(SIGDN_FILESYSPATH, &raw), "Cannot get map path");
            std::wstring path = raw;
            CoTaskMemFree(raw);
            return path;
        }
    }

    std::wstring pickMapFile(HWND owner) {
        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        check(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog)), "Cannot create Open dialog");
        return show(dialog.Get(), owner, FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
    }

    std::wstring pickMapSaveFile(HWND owner, const std::filesystem::path& suggestedPath) {
        Microsoft::WRL::ComPtr<IFileSaveDialog> dialog;
        check(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog)), "Cannot create Save As dialog");
        check(dialog->SetTitle(L"Save injected map as"), "Cannot set Save As title");
        check(dialog->SetFileName(suggestedPath.filename().c_str()), "Cannot suggest output name");
        Microsoft::WRL::ComPtr<IShellItem> folder;
        check(SHCreateItemFromParsingName(std::filesystem::absolute(suggestedPath).parent_path().c_str(),
            nullptr, IID_PPV_ARGS(&folder)), "Cannot open output folder");
        check(dialog->SetFolder(folder.Get()), "Cannot set output folder");
        return show(dialog.Get(), owner, FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST);
    }

}
