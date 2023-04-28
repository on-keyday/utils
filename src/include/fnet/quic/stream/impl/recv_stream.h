/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../resend/fragments.h"
#include "../fragment.h"
#include "../../../error.h"
#include "../../transport_error.h"

namespace utils {
    namespace fnet::quic::stream::impl {

        template <class Locker>
        struct RecvSorted {
            StreamID id;

           private:
            Locker locker;
            slib::list<resend::StreamFragment> sorted;
            bool done = false;
            bool fin = false;
            bool rst = false;
            std::uint64_t read_pos = 0;

            auto pack_new(Fragment frag) {
                resend::StreamFragment save;
                save.data = frag.fragment;
                save.offset = frag.offset;
                save.fin = frag.fin;
                fin = fin || frag.fin;
                return save;
            }

            auto lock() {
                locker.lock();
                return helper::defer([&] {
                    locker.unlock();
                });
            }

            error::Error fragment_error(FrameType type) {
                return QUICError{
                    .msg = "received fragment is not same as received before",
                    .transport_error = TransportError::STREAM_STATE_ERROR,
                    .frame_type = type,
                };
            }

           public:
            bool check_done() {
                if (done) {
                    return true;
                }
                if (!fin) {
                    return false;
                }
                std::uint64_t prev = read_pos;
                for (auto it = sorted.begin(); it != sorted.end(); it++) {
                    if (prev != it->offset) {
                        return false;
                    }
                    prev = it->offset + it->data.size();
                }
                done = true;
                return true;
            }

            std::pair<bool, error::Error> handle_single_range(auto& iter, FrameType type, Fragment frag) {
                resend::StreamFragment& it = *iter;
                auto curmax = it.offset + it.data.size();
                auto newmax = frag.offset + frag.fragment.size();
                // completely duplicated
                if (newmax <= curmax) {
                    auto cmp = it.data.rvec().substr(frag.offset - it.offset, frag.fragment.size());
                    if (cmp != frag.fragment) {
                        auto sub = frag.fragment.substr(0, 30);
                        auto found = std::string_view(it.data.as_char(), it.data.size()).find(sub.as_char(), 0, sub.size());
                        return {false, fragment_error(type)};
                    }
                    fin = fin || frag.fin;
                    return {
                        check_done(),
                        error::none,
                    };
                }
                // partially duplicated
                else if (frag.offset <= curmax) {
                    auto cmp1 = it.data.rvec().substr(frag.offset - it.offset);
                    auto cmp2 = frag.fragment.substr(0, cmp1.size());
                    if (cmp1 != cmp2) {
                        return {false, fragment_error(type)};
                    }
                    const auto add = frag.fragment.substr(cmp1.size());
                    it.data.append(add);
                    it.fin = frag.fin;
                    fin = fin || frag.fin;
                    return {
                        check_done(),
                        error::none,
                    };
                }
                // no duplicated = new fragment
                else {
                    sorted.insert(++iter, pack_new(frag));
                    return {check_done(), error::none};
                }
            }

            std::pair<bool, error::Error> recv(FrameType type, Fragment frag) {
                const auto unlock = lock();
                if (done || rst) {
                    return {true, error::none};
                }
                if (frag.offset < read_pos) {
                    return {true, error::none};  // ignore
                }
                for (auto it = sorted.begin(); it != sorted.end(); it++) {
                    if (it->offset <= frag.offset) {
                        auto cp = it;
                        cp++;
                        if (cp == sorted.end()) {
                            return handle_single_range(it, type, frag);
                        }
                        const auto nextofs = cp->offset;
                        if (nextofs <= frag.offset) {
                            continue;
                        }
                        const auto curmax = it->offset + it->data.size();
                        const auto newmax = frag.offset + frag.fragment.size();
                        if (newmax <= nextofs) {
                            return handle_single_range(it, type, frag);
                        }
                        const auto cmp1 = it->data.rvec().substr(frag.offset - it->offset);
                        const auto cmp2 = frag.fragment.substr(0, cmp1.size());
                        if (cmp1 != cmp2) {
                            return {false, fragment_error(type)};
                        }
                        const auto add = frag.fragment.substr(cmp1.size(), nextofs - curmax);
                        it->data.append(add);
                        auto remain = frag.fragment.substr(nextofs - curmax);
                        frag.fragment = remain;
                        frag.offset = nextofs;
                        continue;
                    }
                }
                if (sorted.size() && frag.offset < sorted.front().offset) {
                    sorted.push_front(pack_new(frag));
                }
                else {
                    sorted.push_back(pack_new(frag));
                }
                return {check_done(), error::none};
            }

            void reset() {
                const auto unlock = lock();
                rst = true;
            }

           private:
            void check_offset() {
                if (sorted.size() > 1) {
                    auto next = ++sorted.begin();
                    assert(sorted.begin()->offset < next->offset);
                }
            }

            std::pair<view::wvec, bool> read_impl(view::wvec data) {
                if (rst) {
                    return {{}, true};
                }
                size_t i = 0;
                auto sub = data;
                while (sorted.size() != 0 && i < data.size()) {
                    auto& tgt = sorted.front();
                    if (tgt.offset != read_pos) {
                        check_offset();
                        break;
                    }
                    if (sorted.front().data.size() > sub.size()) {
                        view::copy(sub, tgt.data);
                        auto new_size = tgt.data.size() - sub.size();
                        view::shift(tgt.data, 0, sub.size(), new_size);
                        tgt.data.resize(new_size);
                        tgt.offset += sub.size();
                        check_offset();
                        read_pos += sub.size();
                        i += sub.size();
                        break;
                    }
                    view::copy(sub, tgt.data);
                    read_pos += tgt.data.size();
                    sub = sub.substr(tgt.data.size());
                    i += tgt.data.size();
                    sorted.pop_front();
                }
                return {data.substr(0, i), sorted.size() == 0 && done};
            }

           public:
            // returns (data,eos)
            std::pair<view::wvec, bool> read(view::wvec data, bool use_try_lock = false) {
                if (use_try_lock) {
                    if (!locker.try_lock()) {
                        return {{}, false};
                    }
                    auto unlock = helper::defer([&] {
                        locker.unlock();
                    });
                    return read_impl(data);
                }
                else {
                    auto unlock = lock();
                    return read_impl(data);
                }
            }

           private:
            std::pair<flex_storage, bool> read_direct_impl() {
                if (rst) {
                    return {{}, true};
                }
                if (sorted.size() == 0) {
                    return {{}, done};
                }
                if (sorted.front().offset != read_pos) {
                    return {{}, done};
                }
                auto data = std::move(sorted.front());
                read_pos += data.data.size();
                sorted.pop_front();
                return {std::move(data.data), sorted.size() == 0 && done};
            }

           public:
            std::pair<flex_storage, bool> read_direct(bool use_try_lock = false) {
                if (use_try_lock) {
                    if (!locker.try_lock()) {
                        return {{}, false};
                    }
                    auto unlock = helper::defer([&] {
                        locker.unlock();
                    });
                    return read_direct_impl();
                }
                else {
                    auto unlock = lock();
                    return read_direct_impl();
                }
            }
        };

        template <class Locker>
        inline std::pair<bool, error::Error> reader_recv_handler(std::shared_ptr<void>& arg, StreamID id, FrameType type, Fragment frag, std::uint64_t total_recv, std::uint64_t err_code) {
            if (!arg) {
                return {false, error::Error("unexpected arg")};
            }
            auto s = static_cast<RecvSorted<Locker>*>(arg.get());
            if (!s->id.valid()) {
                s->id = id;
            }
            if (s->id != id) {
                return {false, error::Error("unexpected bug!!")};
            }
            if (type == FrameType::RESET_STREAM) {
                s->reset();
                return {false, error::none};
            }
            return s->recv(type, frag);
        }
    }  // namespace fnet::quic::stream::impl
}  // namespace utils
