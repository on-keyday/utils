/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <quic/core/core.h>
#include <quic/internal/context_internal.h>

namespace utils {
    namespace quic {
        namespace core {
            struct QQue {
                context::QUIC* get_q(tsize seq) {
                    return nullptr;
                }
            };

            enum class Status {

            };

            struct Looper {
                tsize seq;
                QQue q;
            };

            void progress_loop(Looper* l) {
                auto q = l->q.get_q(l->seq);
                if (!q) {
                    return;  // nothing to do
                }
                q->gcallback.on_enter_progress(l->seq);
            }
        }  // namespace core
    }      // namespace quic
}  // namespace utils