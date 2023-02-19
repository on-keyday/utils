/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// connection_id - connection id manager
#pragma once
#include "../../storage.h"
#include "../../std/hash_map.h"
#include "../ack/ack_lost_record.h"
#include "../frame/conn_id.h"
#include "random.h"

namespace utils {
    namespace fnet::quic::connid {
        struct IDStorage {
            std::int64_t seq = 0;
            storage id;
            byte stateless_reset_token[16];
        };

        struct IDWait {
            IDStorage id;
            std::int64_t retire_proior_to = 0;
            std::shared_ptr<ack::ACKLostRecord> wait;
            bool needless_ack = false;
        };

        struct ConnID {
            std::int64_t seq = -1;
            view::rvec id;
            view::rvec stateless_reset_token;
        };

        struct IDIssuer {
            std::int64_t seq = -1;
            Random random;
            slib::hash_map<std::int64_t, IDWait> idlist;
            bool use_zero_length = false;

            ConnID issue(size_t len, bool needless_ack) {
                if (use_zero_length) {
                    return {};
                }
                if (len == 0) {
                    return {};
                }
                IDStorage id;
                id.id = make_storage(len);
                if (id.id.null()) {
                    return {};
                }
                if (!random.gen_random(id.id)) {
                    return {};
                }
                if (!random.gen_random(id.stateless_reset_token)) {
                    return {};
                }
                id.seq = seq + 1;
                auto [it, ok] = idlist.try_emplace(id.seq, IDWait{std::move(id)});
                if (!ok) {
                    return {};
                }
                ++seq;
                auto& st = it->second.id;
                it->second.needless_ack = needless_ack;
                return {st.seq, st.id, st.stateless_reset_token};
            }

            bool retire(const frame::RetireConnectionIDFrame& retire) {
                return idlist.erase(retire.sequence_number) != 0;
            }

            ConnID choose(std::int64_t sequence_number) {
                if (auto found = idlist.find(sequence_number); found != idlist.end()) {
                    return ConnID{
                        .seq = found->second.id.seq,
                        .id = found->second.id.id,
                        .stateless_reset_token = found->second.id.stateless_reset_token,
                    };
                }
                return {};
            }

            view::rvec pick_up_id() {
                for (auto& id : idlist) {
                    return id.second.id.id;
                }
                return {};
            }
        };

        struct RetireWait {
            std::int64_t seq = -1;
            std::shared_ptr<ack::ACKLostRecord> wait;
        };

        struct IDAcceptor {
            std::int64_t max_seq = -1;

            bool use_zero_length = false;

            slib::hash_map<std::int64_t, IDStorage> idlist;
            slib::hash_map<std::int64_t, RetireWait> retire_wait;

            bool accept(std::int64_t sequence_number, view::rvec connectionID, const byte (&stateless_reset_token)[16]) {
                IDStorage id;
                id.seq = sequence_number;
                id.id = make_storage(connectionID);
                if (id.id.null()) {
                    return false;
                }
                view::copy(id.stateless_reset_token, stateless_reset_token);
                idlist.try_emplace(id.seq, std::move(id));
                if (id.seq > max_seq) {
                    max_seq = id.seq;
                }
                return true;
            }

            ConnID choose(std::int64_t sequence_number) {
                if (auto found = idlist.find(sequence_number); found != idlist.end()) {
                    return ConnID{
                        .seq = found->second.seq,
                        .id = found->second.id,
                        .stateless_reset_token = found->second.stateless_reset_token,
                    };
                }
                return {};
            }

            view::rvec pick_up_id() {
                for (auto& v : idlist) {
                    return v.second.id;  // temporary
                }
                return {};
            }

            bool retire(std::int64_t seq) {
                if (!idlist.erase(seq)) {
                    return false;
                }
                retire_wait.emplace(seq, RetireWait{});
                return true;
            }
        };

    }  // namespace fnet::quic::connid
}  // namespace utils
