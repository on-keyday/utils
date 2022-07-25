/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <quic/common/dll_cpp.h>
#include <quic/internal/context_internal.h>
#include <quic/conn/conn.h>

namespace utils {
    namespace quic {
        namespace conn {
            struct Streams {
            };
            Conn ConnPool::get() {
                Conn c;
                conns.de_q(c);
                if (c) {
                    return c;
                }
                c = mem::make_shared<Connection>(conns.stock.a);
                return c;
            }

            dll(Connection*) new_connection(core::QUIC* q, Mode mode) {
                if (!q) {
                    return nullptr;
                }
                auto self = q->self;
                if (!self) {
                    return nullptr;
                }
                auto c = q->conns.get();
                if (!c) {
                    return nullptr;
                }
                c->q = self;
                c->self = c;
                c->mode = mode;
                return c.get();
            }
        }  // namespace conn
    }      // namespace quic
}  // namespace utils
