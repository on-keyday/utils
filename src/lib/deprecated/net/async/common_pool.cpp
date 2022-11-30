/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <platform/windows/dllexport_source.h>
#include <deprecated/net/async/pool.h>
#include <async/worker.h>
#include <thread>
#include <platform/windows/io_completetion_port.h>

namespace utils {
    namespace net {

        async::TaskPool& STDCALL get_pool() {
            static async::TaskPool netpool;
            return netpool;
        }
#ifdef _WIN32
        std::atomic_bool flag;

        void completion_thread(bool inf) {
            auto iocp = platform::windows::get_iocp();
            while (flag) {
                iocp->wait_callbacks(64, inf ? ~0 : 500);
            }
        }

        void STDCALL set_iocompletion_thread(bool run, bool inf) {
            auto v = flag.exchange(run);
            if (!v) {
                std::thread(completion_thread, inf).detach();
            }
        }
#endif
    }  // namespace net
}  // namespace utils
