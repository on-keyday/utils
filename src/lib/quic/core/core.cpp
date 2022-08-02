/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <quic/common/dll_cpp.h>
#include <quic/core/core.h>
#include <quic/internal/context_internal.h>
#include <atomic>
#include <mutex>
#include <quic/mem/que.h>
#include <quic/io/udp.h>

namespace utils {
    namespace quic {
        namespace core {

            constexpr auto queinit = 10;

            struct QQue {
                mem::LinkQue<mem::shared_ptr<QUIC>> que;
                mem::shared_ptr<QUIC> exhausted;

                bool en_q(mem::shared_ptr<QUIC> q) {
                    return que.init(queinit) && que.en_q(std::move(q));
                }

                mem::shared_ptr<QUIC> de_q() {
                    mem::shared_ptr<QUIC> q = nullptr;
                    que.de_q(q);
                    return q;
                }
            };

            bool EQue::en_q(Event&& e) {
                return que.init(queinit) && que.en_q(std::move(e));
            }

            Event EQue::de_q() {
                Event ev{EventType::idle};
                que.lock_callback([&]() {
                    if (mem_exhausted.arg.type != EventType::idle) {
                        ev = mem_exhausted;
                        mem_exhausted = {EventType::idle};
                        return;
                    }
                    que.de_q_nlock(ev);
                });
                return ev;
            }

            struct EventLoop {
                core::global g;
                tsize seq;
                QQue q;
                std::atomic_flag end;
                QUIC* mem_exhausted;
            };

            dll(void) enque_event(QUIC* q, Event e) {
                if (!q->ev->en_q(std::move(e))) {
                    q->ev->mem_exhausted = e;
                    return;
                }
            }

            Event next(EQue* q) {
                return q->de_q();
            }

            void thread_(EventLoop* l) {
                while (!l->end.test()) {
                    progress_loop(l);
                }
            }

            dll(bool) add_loop(EventLoop* l, QUIC* q) {
                return l->q.en_q(q->self);
            }

            dll(tsize) rem_loop(EventLoop* l, QUIC* q) {
                if (!q || !l) {
                    return 0;
                }
                auto self = q->self;
                return l->q.que.rem_q([&](mem::shared_ptr<QUIC>& cmp) {
                    return cmp.get() == self.get();
                });
            }

            dll(bool) progress_loop(EventLoop* l) {
                auto q = l->q.de_q();
                if (!q) {
                    return false;  // nothing to do
                }
                q->g->gcallback.on_enter_progress(l->seq);
                l->seq++;
                Event event = next(q->ev.get());
                q->g->gcallback.on_event(event.arg);
                if (event.arg.type == EventType::idle) {
                    return false;
                }
                auto qstat = que_remove;
                if (event.callback) {
                    qstat = event.callback(l);
                }
                if (qstat == que_enque) {
                    q->ev->en_q(std::move(event));
                }
                l->q.en_q(std::move(q));
                return true;
            }

            dll(EventLoop*) new_Looper(QUIC* q) {
                if (!q || !q->g) {
                    return nullptr;
                }
                auto l = q->g->alloc.allocate<EventLoop>();
                if (!l) {
                    return nullptr;
                }
                l->g = q->g;
                l->q.que.stock.a = &l->g->alloc;
                return l;
            }

            dll(allocate::Alloc*) get_alloc(QUIC* q) {
                return &q->g->alloc;
            }

            dll(bytes::Bytes) get_bytes(QUIC* q, tsize len) {
                return q->g->bpool.get(len);
            }

            Dll(void) put_bytes(QUIC* q, bytes::Bytes&& b) {
                q->g->bpool.put(std::move(b));
            }

            dll(QUIC*) new_QUIC(allocate::Alloc a) {
                global g = mem::make_inner_alloc<Global>(a, [](Global& g, allocate::Alloc a) {
                    g.alloc = a;
                    return &g.alloc;
                });
                if (!g) {
                    return nullptr;
                }
                auto q = mem::make_shared<QUIC>(&g->alloc);
                if (!q) {
                    return nullptr;
                }
                q->g = g;
                q->self = q;
                q->ev = mem::make_shared<EQue>(&q->g->alloc);
                if (!q->ev) {
                    auto _ = std::move(q->self);
                    return nullptr;
                }
                q->io = mem::make_shared<IOHandles>(&q->g->alloc);
                if (!q->io) {
                    auto _ = std::move(q->self);
                    return nullptr;
                }
                q->ev->que.stock.a = &q->g->alloc;
                q->g->bpool.a = &q->g->alloc;
                q->conns.conns.stock.shared = &q->conns.stock;
                q->conns.alive.stock.shared = &q->conns.stock;
                q->conns.stock.a = &q->g->alloc;
                q->io->ioproc = {};
                return q.get();
            }

            dll(QUIC*) default_QUIC() {
                auto q = new_QUIC(allocate::stdalloc());
                if (!q) {
                    return nullptr;
                }
                register_io(q, io::udp::Protocol(&q->g->alloc));
                set_bpool(q, bytes::stdbpool(q));
                return q;
            }

            dll(void) del_QUIC(QUIC* q) {
                if (!q) {
                    return;
                }
                q->ev->que.destruct([&](Event& e) {
                    e.callback(nullptr);
                });
                auto del_conns = [](conn::Conn& c) {
                    auto _ = std::move(c->self);
                    auto q_ = std::move(c->q);
                };
                q->conns.conns.destruct(del_conns);
                q->conns.alive.destruct(del_conns);
                auto keep_alive_g = q->g;  // for destruction
                auto _ = std::move(q->self);
            }

            dll(void) set_bpool(QUIC* q, pool::BytesHolder h) {
                if (q->g->bpool.holder.free_) {
                    q->g->bpool.holder.free_(q->g->bpool.holder.h, &q->g->alloc);
                }
                q->g->bpool.holder = h;
            }

            dll(void) register_io(QUIC* q, io::IO io) {
                q->io->ioproc.free();
                q->io->ioproc = io;
            }

            Dll(io::IO*) get_io(QUIC* q) {
                return &q->io->ioproc;
            }
        }  // namespace core
    }      // namespace quic
}  // namespace utils
