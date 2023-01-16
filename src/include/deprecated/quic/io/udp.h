/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// udp - wrapper of user datagram protocol
#pragma once
#include "../common/dll_h.h"
#include "../mem/alloc.h"
#include "io.h"
#include "../core/core.h"

namespace utils {
    namespace quic {
        namespace io {
            namespace udp {
                Dll(io::IO) Protocol(allocate::Alloc* a);

                using IOCallback = mem::CBS<void, io::Target*, const byte*, tsize, Result>;

                Dll(Result) enque_io_wait(core::QUIC* q, io::Target t, io::IO* r,
                                          tsize bufsize,
                                          Timeout timeout, IOCallback cb);

                template <class F>
                IOCallback make_iocb(allocate::Alloc* a, F&& f) {
                    return mem::make_cb<void, Target*, const byte*, tsize, Result>(a, std::forward<F>(f));
                }

                constexpr TargetKey UDP_IPv4 = 17;
                constexpr TargetKey UDP_IPv6 = 18;

                constexpr OptionKey MTU = 1;
                constexpr OptionKey Connect = 2;

                Dll(CommonIOWait*) get_common_iowait(void* v);
                Dll(void) del_iowait(core::QUIC* q, void* v);
            }  // namespace udp
        }      // namespace io
    }          // namespace quic
}  // namespace utils
