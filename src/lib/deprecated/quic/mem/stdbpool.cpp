/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <deprecated/quic/common/dll_cpp.h>
#include <deprecated/quic/core/core.h>
#include <deprecated/quic/mem/pool.h>
#include <mutex>
#include <unordered_map>
#include <deprecated/quic/internal/context_internal.h>

namespace utils {
    namespace quic {
        namespace bytes {
            struct BPool {
                std::unordered_multimap<tsize, bytes::Bytes> bp;
                std::mutex m;
            };

            bytes::Bytes BPool_get(void* p, tsize s, allocate::Alloc*) {
                auto b = static_cast<BPool*>(p);
                std::scoped_lock lock{b->m};
                auto v = b->bp.find(s);
                if (v == b->bp.end()) {
                    return {};
                }
                auto ret = std::move(v->second);
                auto f = v;
                auto e = ++v;
                b->bp.erase(f, e);
                return ret;
            }

            void BPool_put(void* p, bytes::Bytes s, allocate::Alloc*) {
                auto b = static_cast<BPool*>(p);
                std::scoped_lock lock{b->m};
                b->bp.emplace(s.size(), std::move(s));
            }

            dll(pool::BytesHolder) stdbpool(core::QUIC* q) {
                pool::BytesHolder bh;
                bh.h = q->g->alloc.allocate<BPool>();
                bh.get_ = BPool_get;
                bh.put_ = BPool_put;
                bh.free_ = [](void* h, allocate::Alloc* a) {
                    auto b = static_cast<BPool*>(h);
                    for (auto& e : b->bp) {
                        a->discard_buffer(e.second);
                    }
                    a->deallocate(b);
                };
                return bh;
            }
        }  // namespace bytes
    }      // namespace quic
}  // namespace utils
