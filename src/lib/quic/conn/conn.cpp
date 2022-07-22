/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <context_internal.h>

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

            Connection* new_connection(core::QUIC* q) {
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
                return c.get();
            }
        }  // namespace conn
    }      // namespace quic
}  // namespace utils
