/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#ifdef _WIN32
#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/platform/windows/io_completetion_port.h"
#include "../../../include/thread/lite_lock.h"

#include "../../../include/helper/iface_cast.h"

#include "../../../include/wrap/lite/map.h"

#include <windows.h>
#include <cassert>

#include <thread>

namespace utils {
    namespace platform {
        namespace windows {

            ::HANDLE iocp = INVALID_HANDLE_VALUE;

            void iocp_pool(size_t handlecount, int wait) {
                OVERLAPPED_ENTRY entry[64];
                DWORD transfered = 0;
                ULONG removed;
                BOOL bRet;
                if (!::GetQueuedCompletionStatusEx(iocp, entry, handlecount, &removed, wait, false)) {
                    auto err = ::GetLastError();
                }
            }

            IOCPObject* start_iocp() {
                static IOCPObject obj;
                return &obj;
            }

            bool set_handle(void* handle) {
                auto res = ::CreateIoCompletionPort(handle, iocp,
                                                    0, 0);
                return res != nullptr;
            }

            bool IOCPObject::register_handle(void* handle) {
                if (!handle || handle == INVALID_HANDLE_VALUE) {
                    return false;
                }
                return set_handle(handle);
            }

            void init_iocp() {
                iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL,
                                                0, 0);
                assert(iocp);
            }

            IOCPObject::IOCPObject() {
                init_iocp();
            }

            IOCPObject::~IOCPObject() {}

        }  // namespace windows

    }  // namespace platform
}  // namespace utils
#endif