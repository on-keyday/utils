/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../../include/platform/windows/io_completetion_port.h"

#include <windows.h>
#include <cassert>

#include <thread>

namespace utils {
    namespace platform {
        namespace windows {

            struct IOCPContext {
                ::HANDLE handle = INVALID_HANDLE_VALUE;
                size_t id = 0;
            } context;

            constexpr size_t exit_msg = ~0;

            void clean_iocp_thread(IOCPContext* ctx) {
                for (auto i = 0; i < std::thread::hardware_concurrency(); i++) {
                    auto ptr = new ::OVERLAPPED{};
                    PostQueuedCompletionStatus(ctx->handle, 0, exit_msg, ptr);
                }
            }

            void iocp_thread(IOCPContext* ctx) {
                LPOVERLAPPED pol = NULL;
                DWORD transfered = 0;
                ULONG_PTR key;
                BOOL bRet;
                while (true) {
                    if (!::GetQueuedCompletionStatus(ctx->handle, &transfered, &key, &pol, INFINITE)) {
                        continue;
                    }
                    if (key == exit_msg) {
                        delete pol;
                        break;
                    }
                }
            }

            void start_iocp() {
                context.handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL,
                                                          0, 0);
                assert(context.handle);
                for (auto i = 0; i < std::thread::hardware_concurrency(); i++) {
                    std::thread(iocp_thread).detach();
                }
            }

            void set_handle(void* handle) {
            }

        }  // namespace windows

    }  // namespace platform
}  // namespace utils