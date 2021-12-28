/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


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

            struct IOCPContext {
                ::HANDLE handle = INVALID_HANDLE_VALUE;
                size_t id = 0;
            } context;

            wrap::map<size_t, Complete> callbacks;

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
                    helper::Unsafe<Complete> cvt;
                    cvt.ptr = reinterpret_cast<void*>(key);
                    cvt.iface(transfered);
                }
            }

            IOCPObject* start_iocp() {
                static IOCPObject obj;
                return &obj;
            }

            thread::LiteLock glock;

            void set_handle(void* handle, void* completed) {
                auto res = ::CreateIoCompletionPort(handle, context.handle,
                                                    ULONG_PTR(completed), 0);
                assert(res == context.handle);
                return;
            }

            bool IOCPObject::register_handler(void* handle, Complete&& complete) {
                if (!handle || handle == INVALID_HANDLE_VALUE || !complete) {
                    return false;
                }
                helper::Unsafe<Complete> cvt;
                cvt.iface = std::move(complete);
                set_handle(handle, cvt.ptr);
                return true;
            }

            void start_iocp(IOCPContext* ctx) {
                ctx->handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL,
                                                       0, 0);
                assert(ctx->handle);
                for (auto i = 0; i < std::thread::hardware_concurrency(); i++) {
                    std::thread(iocp_thread, ctx).detach();
                }
            }

            IOCPObject::IOCPObject() {
                ctx = &context;
                start_iocp(ctx);
            }

            IOCPObject::~IOCPObject() {
                clean_iocp_thread(ctx);
            }

        }  // namespace windows

    }  // namespace platform
}  // namespace utils