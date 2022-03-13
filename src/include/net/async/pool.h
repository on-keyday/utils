/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pool - net woker pool
#pragma once

#include "../../platform/windows/dllexport_header.h"
#include "../../async/worker.h"

namespace utils {
    namespace net {
        DLL async::TaskPool& STDCALL get_pool();

        template <class Fn, class... Args>
        decltype(auto) start(Fn&& fn, Args&&... args) {
            return async::start(get_pool(), std::forward<Fn>(fn), std::forward<Args>(args)...);
        }

#ifdef _WIN32
        DLL void STDCALL set_iocompletion_thread(bool run);
#else
        inline void set_iocompletion_thread(bool) {}
#endif
    }  // namespace net
}  // namespace utils
