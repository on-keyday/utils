/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "stream.h"

namespace utils {
    namespace fnet::quic::stream::impl {

        // this is helper to bind ConnHandler implementation to ConnHandler<TConfig>
        // for example:
        // template<class TConfig>
        // class ConnHandler = Bind<ConnHandlerSingleArg,void*>::template bound
        template <template <class...> class T, class... Arg>
        struct Bind {
            template <class TConfig>
            using bound = T<TConfig, Arg...>;
        };

        struct AutoIncreaser {
            bool auto_increase_uni_stream = false;
            bool auto_increase_bidi_stream = false;
            bool auto_increase_max_data = false;

            void auto_incrase(auto t) {
                if (auto_increase_bidi_stream) {
                    auto a = t->remote_bidi_avail();
                    t->update_max_bidi_streams([&](core::Limiter lim, std::uint64_t ini_size) {
                        if (lim.avail_size() + a < ini_size) {
                            return lim.current_limit() + 1;
                        }
                        return lim.current_limit();
                    });
                }
                if (auto_increase_uni_stream) {
                    auto a = t->remote_uni_avail();
                    t->update_max_bidi_streams([&](core::Limiter lim, std::uint64_t ini_size) {
                        if (lim.avail_size() + a < ini_size) {
                            return lim.current_limit() + 1;
                        }
                        return lim.current_limit();
                    });
                }
                if (auto_increase_max_data) {
                    t->update_max_data([&](core::Limiter lim, std::uint64_t ini_size) {
                        if (lim.avail_size() < ini_size) {
                            return lim.current_limit() + ini_size - lim.avail_size();
                        }
                        return lim.current_limit();
                    });
                }
            }
        };

        // TConfig is passed by Conn
        template <class TConfig, class Arg>
        struct ConnHandlerSingleArg {
            using UniOpenCB = void (*)(Arg& arg, std::shared_ptr<SendUniStream<TConfig>> stream);
            using UniAcceptCB = void (*)(Arg& arg, std::shared_ptr<RecvUniStream<TConfig>> stream);
            using BidiOpenCB = void (*)(Arg& arg, std::shared_ptr<BidiStream<TConfig>> stream);
            using BidiAcceptCB = void (*)(Arg& arg, std::shared_ptr<BidiStream<TConfig>> stream);
            using SendScheduleCB = IOResult (*)(Arg& arg, frame::fwriter& fw, ack::ACKRecorder& rec);

            Arg arg{};
            UniOpenCB uni_open_cb = nullptr;
            UniAcceptCB uni_accept_cb = nullptr;
            BidiOpenCB bidi_open_cb = nullptr;
            BidiAcceptCB bidi_accept_cb = nullptr;
            SendScheduleCB send_schedule_cb = nullptr;
            AutoIncreaser auto_incr;

            void uni_open(std::shared_ptr<SendUniStream<TConfig>> s) {
                if (!uni_open_cb) {
                    return;
                }
                uni_open_cb(arg, std::move(s));
            }

            void uni_accept(std::shared_ptr<RecvUniStream<TConfig>> s) {
                if (!uni_accept_cb) {
                    return;
                }
                uni_accept_cb(arg, std::move(s));
            }

            void bidi_open(std::shared_ptr<BidiStream<TConfig>> s) {
                if (!bidi_open_cb) {
                    return;
                }
                bidi_open_cb(arg, std::move(s));
            }

            void bidi_accept(std::shared_ptr<BidiStream<TConfig>> s) {
                if (!bidi_accept_cb) {
                    return;
                }
                bidi_accept_cb(arg, std::move(s));
            }

            IOResult send_schedule(frame::fwriter& fw, ack::ACKRecorder& observer, auto&& do_default) {
                if (send_schedule_cb) {
                    return send_schedule_cb(arg, fw, observer);
                }
                return do_default();
            }

            void recv_callback(auto d) {
                auto_incr.auto_incrase(d);
            }

            void send_callback(auto d) {
                auto_incr.auto_incrase(d);
            }

            void set_arg(Arg&& a) {
                arg = std::move(a);
            }

            void set_open_bidi(BidiOpenCB cb) {
                bidi_open_cb = cb;
            }

            void set_accept_bidi(BidiOpenCB cb) {
                bidi_accept_cb = cb;
            }

            void set_open_uni(UniOpenCB cb) {
                uni_open_cb = cb;
            }

            void set_accept_uni(UniAcceptCB cb) {
                uni_accept_cb = cb;
            }
        };

        template <class A, class B, class C, class D>
        struct SchedArgSet {
            A& uni_open;
            B& uni_accept;
            C& bidi_open;
            D& bidi_accept;
        };

        template <class TConfig,
                  class UniOpenArg,
                  class UniAcceptArg,
                  class BidiOpenArg,
                  class BidiAcceptArg>
        struct ConnHandlerEach {
            using SchedArg = SchedArgSet<UniOpenArg, UniAcceptArg, BidiOpenArg, BidiAcceptArg>;
            using UniOpenCB = void (*)(UniOpenArg& arg, std::shared_ptr<SendUniStream<TConfig>> stream);
            using UniAcceptCB = void (*)(UniAcceptArg& arg, std::shared_ptr<RecvUniStream<TConfig>> stream);
            using BidiOpenCB = void (*)(BidiOpenArg& arg, std::shared_ptr<BidiStream<TConfig>> stream);
            using BidiAcceptCB = void (*)(BidiAcceptArg& arg, std::shared_ptr<BidiStream<TConfig>> stream);
            using SendScheduleCB = IOResult (*)(SchedArg& arg, frame::fwriter& fw, ack::ACKRecorder& rec);

            UniOpenArg uni_open_arg{};
            UniAcceptArg uni_accept_arg{};
            BidiOpenArg bidi_open_arg{};
            BidiAcceptArg bidi_accept_arg{};
            UniOpenCB uni_open_cb = nullptr;
            UniAcceptCB uni_accept_cb = nullptr;
            BidiOpenCB bidi_open_cb = nullptr;
            BidiAcceptCB bidi_accept_cb = nullptr;
            SendScheduleCB send_schedule_cb = nullptr;
            AutoIncreaser auto_incr;

            void uni_open(std::shared_ptr<SendUniStream<TConfig>> s) {
                if (!uni_open_cb) {
                    return;
                }
                uni_open_cb(uni_open_arg, std::move(s));
            }

            void uni_accept(std::shared_ptr<RecvUniStream<TConfig>> s) {
                if (!uni_accept_cb) {
                    return;
                }
                uni_accept_cb(uni_accept_arg, std::move(s));
            }

            void bidi_open(std::shared_ptr<BidiStream<TConfig>> s) {
                if (!bidi_open_cb) {
                    return;
                }
                bidi_open_cb(bidi_open_arg, std::move(s));
            }

            void bidi_accept(std::shared_ptr<BidiStream<TConfig>> s) {
                if (!bidi_accept_cb) {
                    return;
                }
                bidi_accept_cb(bidi_accept_arg, std::move(s));
            }

            IOResult send_schedule(frame::fwriter& w, ack::ACKRecorder& observer, auto&& do_default) {
                if (send_schedule_cb) {
                    SchedArg arg{uni_open_arg, uni_accept_arg, bidi_open_arg, bidi_accept_arg};
                    return send_schedule_cb(arg, w, observer);
                }
                return do_default();
            }

            void recv_callback(auto d) {
                auto_incr.auto_incrase(d);
            }

            void send_callback(auto d) {
                auto_incr.auto_incrase(d);
            }
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace utils
