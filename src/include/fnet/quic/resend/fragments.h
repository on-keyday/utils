/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// retransmit - retransmit methods
#pragma once
#include "../../std/list.h"
#include "../ack/ack_lost_record.h"
#include "../../storage.h"
#include "../ioresult.h"
#include "ack_handler.h"

namespace futils {
    namespace fnet::quic::resend {

        struct StreamFragment {
            std::uint64_t offset = 0;
            flex_storage data;
            bool fin = false;
        };

        struct CryptoFragment {
            std::uint64_t offset = 0;
            flex_storage data;
        };

        template <class T>
        struct Resend {
            T frag;
            ACKHandler wait;
        };

        template <class Fragment, template <class...> class List>
        struct Retransmiter {
           private:
            List<Resend<Fragment>> fragments;

            static void save_new_frag(auto&& fragset, Fragment&& frag, auto&& observer) {
                Resend<Fragment> data;
                data.frag = std::move(frag);
                data.wait.wait(observer);
                fragset.push_back(std::move(data));
            }

           public:
            void reset() {
                fragments.clear();
            }

            // returns lost_count
            size_t detect_ack_and_lost() {
                size_t i = 0;
                for (auto it = fragments.begin(); it != fragments.end();) {
                    if (it->wait.is_ack()) {
                        it->wait.confirm();
                        it = fragments.erase(it);
                        continue;
                    }
                    if (it->wait.is_lost()) {
                        i++;
                    }
                    it++;
                }
                return i;
            }

            // retransmit retransmits sent data
            // send should be IOResult(Fragment&,auto save_new)
            // save_new is void(Fragment&&)
            // send should return:
            // IOResult::fatal or IOResult::invalid_data - fatal error
            // IOResult::no_capacity - break without deletion
            // IOResult::not_in_io_state - continue without deletion
            // otherwise(IOResult::ok) - continue with deletion
            // this returns IOResult::ok if succeeded
            // if otherwise is returned, fatal error
            IOResult retransmit(auto&& observer, auto&& send) {
                List<Resend<Fragment>> tmpfrags;
                for (auto it = fragments.begin(); it != fragments.end();) {
                    if (it->wait.is_ack()) {
                        it->wait.confirm();
                        it = fragments.erase(it);
                        continue;
                    }
                    if (it->wait.is_lost()) {
                        auto save_new = [&](Fragment&& frag) {
                            save_new_frag(tmpfrags, std::move(frag), observer);
                        };
                        IOResult res = send(it->frag, save_new);
                        if (res == IOResult::fatal || res == IOResult::invalid_data) {
                            return res;
                        }
                        else if (res == IOResult::no_capacity) {
                            break;
                        }
                        else if (res == IOResult::not_in_io_state) {
                            it->wait.confirm();
                            it->wait.wait(observer);
                        }
                        else {
                            it = fragments.erase(it);
                            continue;
                        }
                    }
                    it++;
                }
                fragments.splice(fragments.end(), tmpfrags);
                return IOResult::ok;
            }

            void sent(auto&& observer, Fragment&& frag) {
                save_new_frag(fragments, std::move(frag), observer);
            }

            size_t remain() const {
                return fragments.size();
            }
        };
    }  // namespace fnet::quic::resend
}  // namespace futils
