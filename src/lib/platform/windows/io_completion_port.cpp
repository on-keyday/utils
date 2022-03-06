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

#include "../../../include/wrap/lite/vector.h"

#include <windows.h>
#include <cassert>

#include <thread>

namespace utils {
    namespace platform {
        namespace windows {

            ::HANDLE iocp = INVALID_HANDLE_VALUE;

            wrap::vector<CCBRegistered> callbacks;

            template <class Func>
            void iocp_poll(Func&& fn, size_t handlecount, int wait) {
                OVERLAPPED_ENTRY entry[64];
                ULONG removed;
                if (!::GetQueuedCompletionStatusEx(iocp, entry, handlecount, &removed, wait, false)) {
                    return;
                }
                for (auto p = 0; p < removed; p++) {
                    fn.call(entry[p].lpOverlapped, entry[p].dwNumberOfBytesTransferred);
                }
            }

            void IOCPObject::wait_completion_impl(CCBInvoke cb, size_t maxcount, int wait) {
                if (maxcount > 64) {
                    maxcount = 8;
                }
                iocp_poll(cb, maxcount, wait);
            }

            void IOCPObject::register_callback_impl(CCBRegistered cb) {
                callbacks.push_back(std::move(cb));
            }

            void IOCPObject::wait_callbacks(size_t maxcount, int wait) {
                if (maxcount > 64) {
                    maxcount = 64;
                }
                auto fn = [](void* ol, size_t maxcount) {
                    for (auto& cb : callbacks) {
                        if (cb.call(ol, maxcount)) {
                            return;
                        }
                    }
                };
                iocp_poll(CompletionCallback<decltype(fn)>{fn},
                          maxcount, wait);
            }

            IOCPObject* get_iocp() {
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