/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../ack/ack_lost_record.h"

namespace utils {
    namespace fnet::quic::resend {
        struct ACKHandler {
           private:
            std::shared_ptr<ack::ACKLostRecord> record;

           public:
            void reset() {
                record = nullptr;
            }

            bool is_ack() const {
                if (!record) {
                    return false;
                }
                return record->is_ack();
            }

            bool is_lost() const {
                if (!record) {
                    return false;
                }
                return record->is_lost();
            }

            bool not_confirmed() const {
                return record != nullptr;
            }

            // this clear record
            void confirm() {
                record = nullptr;
            }

            // this clear record and set new record from rec
            // TODO(on-keyday): use ACKRecorder instead
            void wait(ack::ACKRecorder& rec) {
                record = rec.record();
                // record = ack::make_ack_wait();
                // rec.push_back(record);
            }
        };
    }  // namespace fnet::quic::resend
}  // namespace utils
