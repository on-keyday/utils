/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <wrap/admin.h>
#include <wrap/exepath.h>
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <Windows.h>
#include <ShlObj.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <helper/defer.h>
#endif

namespace utils::wrap {
#ifdef UTILS_PLATFORM_WINDOWS
    RunResult run_this_as_admin() {
        if (IsUserAnAdmin()) {
            return RunResult::already_admin;
        }
        auto path = get_exepath<utils::wrap::path_string>();
        auto cmdline = GetCommandLineW();
        SHELLEXECUTEINFOW info{};
        info.lpVerb = L"runas";
        info.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE | SEE_MASK_FLAG_NO_UI;
        info.lpFile = path.c_str();
        info.lpParameters = cmdline;
        info.hInstApp = 0;
        info.nShow = SW_SHOW;
        info.cbSize = sizeof(info);
        if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) != S_OK) {
            return RunResult::failed;
        }
        if (!ShellExecuteExW(&info)) {
            return RunResult::failed;
        }
        WaitForSingleObject(info.hProcess, -1);
        CloseHandle(info.hProcess);
        return RunResult::started;
    }
#else
    RunResult run_this_as_admin() {
        return RunResult::failed;
    }
#endif
}  // namespace utils::wrap
