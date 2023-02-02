/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ack_lost_record - records ack or lost
#pragma once
#include <atomic>
#include "../../../core/byte.h"
#include <memory>
#include "../../dll/allocator.h"

namespace utils {
    namespace dnet::quic::ack {
        enum class ACKLostState : byte {
            wait,
            ack,
            lost,
        };

        struct ACKLostRecord {
           private:
            std::atomic<ACKLostState> state{ACKLostState::wait};

           public:
            constexpr ACKLostRecord() = default;

            void wait() {
                state.store(ACKLostState::wait);
            }

            void ack() {
                state.store(ACKLostState::ack);
            }

            void lost() {
                state.store(ACKLostState::lost);
            }

            bool is_ack() const {
                return state.load() == ACKLostState::ack;
            }

            bool is_lost() const {
                return state.load() == ACKLostState::lost;
            }
        };

        std::shared_ptr<ACKLostRecord> make_ack_wait() {
            return std::allocate_shared<ACKLostRecord>(glheap_allocator<ACKLostRecord>{});
        }

        void mark_as_lost(auto&& vec) {
            for (std::weak_ptr<ACKLostRecord>& w : vec) {
                if (auto got = w.lock()) {
                    got->lost();
                }
            }
        }

        void mark_as_ack(auto&& vec) {
            for (std::weak_ptr<ACKLostRecord>& w : vec) {
                if (auto got = w.lock()) {
                    got->ack();
                }
            }
        }

        struct ACKCollector {
           private:
            void* p = nullptr;
            void (*pb)(void*, std::shared_ptr<ACKLostRecord>&&) = nullptr;
            template <class Ptr>
            constexpr static void pb_(void* p, std::shared_ptr<ACKLostRecord>&& r) {
                static_cast<Ptr>(p)->push_back(std::move(r));
            }

           public:
            constexpr ACKCollector() = default;
            constexpr ACKCollector(auto&& d)
                : p(std::addressof(d)), pb(pb_<decltype(std::addressof(d))>) {}

            constexpr void push_back(auto&& b) {
                pb(p, std::forward<decltype(b)>(b));
            }
        };
    }  // namespace dnet::quic::ack
}  // namespace utils
