/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "external/random.h"
#include "../../storage.h"

namespace futils {
    namespace fnet::quic::connid {

        struct SequenceNumber {
            std::int64_t seq = -1;

            constexpr SequenceNumber() = default;

            constexpr SequenceNumber(std::int64_t n)
                : seq(n) {}

            constexpr bool valid() const noexcept {
                return seq >= 0;
            }

            constexpr operator std::int64_t() const noexcept {
                return seq;
            }
        };

        constexpr auto invalid_seq = SequenceNumber();

        using StatelessResetToken = byte[16];

        struct ConnID {
            SequenceNumber seq = invalid_seq;
            view::rvec id;
            view::rvec stateless_reset_token;
        };

        struct IDStorage {
            SequenceNumber seq = invalid_seq;
            storage id;
            StatelessResetToken stateless_reset_token;

            ConnID to_ConnID() const {
                return ConnID{
                    .seq = seq,
                    .id = id,
                    .stateless_reset_token = stateless_reset_token,
                };
            }
        };

        constexpr StatelessResetToken null_stateless_reset{};

        struct CloseID {
            storage id;
            StatelessResetToken token;
        };

        // CommonParam is common parameters both Issuer and Acceptor
        struct CommonParam {
            Random random;
            std::uint32_t version = 0;
        };

        struct InitialRetry {
           private:
            storage initial_random;
            storage retry_random;

            bool gen(storage& target, byte size, Random& rand, RandomUsage usage) {
                if (!rand.valid()) {
                    return false;
                }
                if (size < 8) {
                    size = 8;
                }
                target = make_storage(size);
                return target.size() == size && rand.gen_random(target, usage);
            }

            bool recv(view::rvec id, storage& target) {
                target = make_storage(id);
                return target.size() == id.size();
            }

           public:
            void reset() {
                initial_random.clear();
                retry_random.clear();
            }

            bool gen_initial(bool is_server, byte size, Random& rand) {
                if (is_server) {
                    return false;
                }
                return gen(initial_random, size, rand, RandomUsage::original_dst_id);
            }

            bool gen_retry(bool is_server, byte size, Random& rand) {
                if (is_server) {
                    return false;
                }
                return gen(retry_random, size, rand, RandomUsage::retry_id);
            }

            bool recv_initial(bool is_server, view::rvec id) {
                if (!is_server) {
                    return false;
                }
                return recv(id, initial_random);
            }

            bool recv_retry(bool is_server, view::rvec id) {
                if (is_server) {
                    return false;
                }
                return recv(id, retry_random);
            }

            bool has_retry() const {
                return retry_random.size() != 0;
            }

            view::rvec get_initial() const {
                return initial_random;
            }

            view::rvec get_retry() const {
                return retry_random;
            }

            view::rvec get_initial_or_retry() const {
                if (has_retry()) {
                    return retry_random;
                }
                else {
                    return initial_random;
                }
            }
        };

    }  // namespace fnet::quic::connid
}  // namespace futils
