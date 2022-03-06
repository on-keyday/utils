/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/async/pool.h"
#include "../../../include/async/worker.h"
#include <thread>
#include "../../../include/platform/windows/io_completetion_port.h"

namespace utils {
    namespace net {

        async::TaskPool& STDCALL get_pool() {
            static async::TaskPool netpool;
            return netpool;
        }
#ifdef _WIN32
        std::atomic_bool flag;

        void completion_thread() {
            auto iocp = platform::windows::get_iocp();
            while (flag) {
                iocp->wait_callbacks(64, 500);
            }
        }

        void STDCALL set_iocompletion_thread(bool run) {
            auto v = flag.exchange(run);
            if (!v) {
                std::thread(completion_thread).detach();
            }
        }
#endif
    }  // namespace net
}  // namespace utils
