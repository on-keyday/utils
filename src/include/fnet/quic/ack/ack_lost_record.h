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
    namespace fnet::quic::ack {
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

        inline std::shared_ptr<ACKLostRecord> make_ack_wait() {
            return std::allocate_shared<ACKLostRecord>(glheap_allocator<ACKLostRecord>{});
        }

        struct ACKRecorder {
           private:
            std::shared_ptr<ACKLostRecord> rec;

           public:
            constexpr ACKRecorder() = default;

            std::shared_ptr<ACKLostRecord> record() {
                if (!rec) {
                    rec = ack::make_ack_wait();
                }
                return rec;
            }

            std::shared_ptr<ACKLostRecord> get() const {
                return rec;
            }
        };

        void mark_as_lost(auto&& vec) {
            std::shared_ptr<ACKLostRecord> r = vec.lock();
            if (!r) {
                return;  // nothing to do
            }
            r->lost();
        }

        void mark_as_ack(auto&& vec) {
            std::shared_ptr<ACKLostRecord> r = vec.lock();
            if (!r) {
                return;  // nothing to do
            }
            r->ack();
        }

    }  // namespace fnet::quic::ack
}  // namespace utils
