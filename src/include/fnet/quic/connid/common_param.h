/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "external/random.h"
#include "../../storage.h"

namespace utils {
    namespace fnet::quic::connid {
        // CommonParam is common parameters both Issuer and Acceptor
        struct CommonParam {
            Random random;
            std::uint32_t version = 0;
        };

        struct InitialRetry {
           private:
            storage initial_random;
            storage retry_random;

            bool gen(storage& target, byte size, Random& rand) {
                if (!rand.valid()) {
                    return false;
                }
                if (size < 8) {
                    size = 8;
                }
                target = make_storage(size);
                return target.size() == size && rand.gen_random(target);
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
                return gen(initial_random, size, rand);
            }

            bool gen_retry(bool is_server, byte size, Random& rand) {
                if (is_server) {
                    return false;
                }
                return gen(retry_random, size, rand);
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

    }  // namespace fnet::quic::connid
}  // namespace utils
