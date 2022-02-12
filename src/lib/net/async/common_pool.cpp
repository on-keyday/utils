/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/async/dns.h"
#include "../../../include/async/worker.h"

namespace utils {
    namespace net {
        async::TaskPool netpool;
    }
}  // namespace utils
