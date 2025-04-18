/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// rewrite include path below using <> instead of ""
#include <platform/windows/dllexport_source.h>
#include <unicode/utf/convert.h>
#include <wrap/light/char.h>
#include <testutil/alloc_hook.h>
#include <testutil/timer.h>
#include <strutil/append.h>
#include <number/to_string.h>
#include <number/insert_space.h>
#include <platform/detect.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <crtdbg.h>
#include <windows.h>
#endif

namespace futils {
    namespace test {
#ifdef FUTILS_PLATFORM_WINDOWS
#ifdef _DEBUG
        _CRT_ALLOC_HOOK base_alloc_hook;
        std::int64_t total_alloced;
        size_t count = 0;
        void* dumpfile;

        Timer t;
        int old_flag;

        Hooker log_hooker;

        int reqfirst = -1;

        struct DumpFileCloser {
            ~DumpFileCloser() {
                if (dumpfile)
                    ::CloseHandle(dumpfile);
            }
        } closer;

        bool STDCALL set_log_file(const char* file) {
            number::Array<wrap::path_char, 1024, true> file_{0};
            utf::convert(file, file_);
            if (dumpfile) {
                ::CloseHandle(dumpfile);
            }
            dumpfile = ::CreateFileW(file_.c_str(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (dumpfile == INVALID_HANDLE_VALUE) {
                dumpfile = nullptr;
            }
            return dumpfile != nullptr;
        }

        int alloc_hook(int nAllocType, void* pvData,
                       size_t nSize, int nBlockUse, long lRequest,
                       const unsigned char* szFileName, int nLine) {
            auto res = base_alloc_hook(nAllocType, pvData, nSize, nBlockUse, lRequest, szFileName, nLine);
            long long delta = 0;
            auto save_log = [&](auto name) {
                number::Array<char, 110> arr{0};
                strutil::appends(arr, name, ":/ptr:");
                number::insert_space(arr, 2 * sizeof(std::uintptr_t), std::uintptr_t(pvData),16);
                number::to_string(arr, std::uintptr_t(pvData),16);
                strutil::appends(arr, "/size:");
                number::insert_space(arr, 7, nSize);
                number::to_string(arr, nSize);
                strutil::appends(arr, "/req:");
                number::insert_space(arr, 6, lRequest);
                number::to_string(arr, lRequest);
                strutil::append(arr, "/total:");
                number::insert_space(arr, 8, total_alloced);
                number::to_string(arr, total_alloced);
                strutil::append(arr, "/time: ");
                delta = t.delta<std::chrono::microseconds>().count();
                number::insert_space(arr, 8, delta);
                number::to_string(arr, delta);
                strutil::append(arr, "/count: ");
                number::to_string(arr, count);
                strutil::append(arr, "\n");
                OutputDebugStringA(arr.buf);
                if (dumpfile) {
                    DWORD w;
                    ::WriteFile(dumpfile, arr.buf, arr.size(), &w, nullptr);
                }
                count++;
            };
            auto callback = [&](HookType type) {
                if (log_hooker) {
                    HookInfo info;
                    info.reqid = lRequest;
                    info.size = nSize;
                    info.time = delta;
                    info.type = type;
                    log_hooker(info);
                }
            };
            if (nAllocType == _HOOK_ALLOC) {
                if (reqfirst == -1) {
                    reqfirst = lRequest;
                }
                total_alloced += nSize;
                save_log("malloc");
                callback(HookType::alloc);
            }
            else if (nAllocType == _HOOK_FREE) {
                if (pvData) {
                    const char* p = "dealoc";
                    if (!_CrtIsValidHeapPointer(pvData)) {
                        p = "invalid dealoc";
                    }
                    else {
                        nSize = _msize_dbg(pvData, _NORMAL_BLOCK);
                        _CrtIsMemoryBlock(pvData, nSize,
                                          &lRequest, nullptr, nullptr);
                        if (reqfirst == -1 || lRequest < reqfirst) {
                            return res;
                        }
                        total_alloced -= nSize;
                    }
                    save_log(p);
                    callback(HookType::dealloc);
                }
            }
            return res;
        }

        void STDCALL set_alloc_hook(bool on) {
            if (on) {
                old_flag = _CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
                base_alloc_hook = _CrtGetAllocHook();
                _CrtSetAllocHook(alloc_hook);
            }
            else {
                if (dumpfile) {
                    ::CloseHandle(dumpfile);
                    dumpfile = nullptr;
                }
                _CrtSetAllocHook(base_alloc_hook);
                base_alloc_hook = nullptr;
                _CrtSetDbgFlag(old_flag);
            }
        }
#endif
#endif
    }  // namespace test
}  // namespace futils
