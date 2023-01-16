/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream_impl - stream controller impl
#pragma once
#include "stream.h"
#include <memory>
#include "../../dll/allocator.h"
#include "../../easy/deque.h"
#include "../../easy/vector.h"
#include "../../easy/hashmap.h"
#include "../../httpstring.h"
#include <algorithm>

namespace utils {
    namespace dnet {
        namespace quic::stream::impl {

            struct SendBuffer {
                String buf;
                size_t offset = 0;
                size_t discarded = 0;
                bool should_fin = false;
                bool fin_set = false;
                easy::Deque<std::shared_ptr<StreamFragment>> fragment;

                bool allocate_fragment(StreamBuf& sbuf) {
                    if (buf.size() == offset && (!should_fin || fin_set)) {
                        return false;
                    }
                    auto b = reinterpret_cast<byte*>(buf.text());
                    sbuf.buf = {b + (offset - discarded), buf.size() + discarded - offset};
                    sbuf.offset = offset;
                    sbuf.fin = should_fin && !fin_set;
                    return true;
                }

                void decide_fragment(StreamFragment frag) {
                    offset += frag.data.len;
                    fin_set = fin_set || frag.fin;
                }

                std::shared_ptr<StreamFragment> send_packet(StreamFragment frag) {
                    auto ptr = std::allocate_shared_for_overwrite<StreamFragment>(glheap_allocator<StreamFragment>{});
                    *ptr = frag;
                    fragment.push_back(std::as_const(ptr));
                    return ptr;
                }

                bool allocate_retransmit_fragment(StreamFragment& frag) {
                    auto cmp = [](auto& a, auto& b) {
                        auto a_ = a->lost ? 1 : 0;
                        auto b_ = b->lost ? 1 : 0;
                        return a_ > b_;
                    };
                    if (!std::is_sorted(fragment.begin(), fragment.end(), cmp)) {
                        std::stable_sort(fragment.begin(), fragment.end(), cmp);
                    }
                    if (fragment.size() == 0 || !fragment[0]->lost) {
                        return false;
                    }
                    frag = *fragment[0];
                    return true;
                }

                void retransmit_fragment(StreamFragment frag) {
                    assert(frag.offset == fragment[0]->offset);
                    fragment.pop_front();
                }
            };

            struct RecvFragment {
                BoxByteLen frag;
                size_t offset = 0;
            };

            struct RecvBuffer {
                easy::Vec<RecvFragment> frag;

                error::Error recv(size_t offset, ByteLen data) {
                    auto cmp = [](auto& a, auto& b) {
                        return a.offset < b.offset;
                    };
                    if (!std::is_sorted(frag.begin(), frag.end(), cmp)) {
                        std::sort(frag.begin(), frag.end(), cmp);
                    }
                    for (auto& f : frag) {
                        if (f.offset == offset) {
                            if (f.frag.len() != data.len) {
                                return QUICError{
                                    .msg =
                                        "this implementation cannot handle same offset different length receipt"
                                        "is this library bug?",
                                };
                            }
                            return err_known_offset;
                        }
                    }
                    RecvFragment new_frag;
                    new_frag.offset = offset;
                    new_frag.frag = data;
                    if (!new_frag.frag.valid()) {
                        return error::memory_exhausted;
                    }
                    frag.push_back(std::move(new_frag));
                    return error::none;
                }
            };

            struct BidiInfo {
                BidiStream st;
                SendBuffer sbuf;
                RecvBuffer rbuf;
            };

            struct UniRecvInfo {
                RecvUniStream st;
                RecvBuffer rbuf;
            };

            struct UniSendInfo {
                SendUniStream st;
                SendBuffer sbuf;
            };

            struct PacketNumberFrag {
                easy::Vec<std::shared_ptr<StreamFragment>> frags;
            };

            struct ImplHolder {
                Direction self_dir = Direction::client;
                easy::H<size_t, BidiInfo> bidi_local;
                easy::H<size_t, BidiInfo> bidi_peer;
                easy::H<size_t, UniSendInfo> uni_send;
                easy::H<size_t, UniRecvInfo> uni_recv;
                easy::H<size_t, PacketNumberFrag> frags;
                BidiInfo* bl_cache = nullptr;
                BidiInfo* bp_cache = nullptr;
                UniSendInfo* us_cache = nullptr;
                UniRecvInfo* ur_cache = nullptr;
            };

            struct ControllerImpl : DummyController {
                using I = ImplHolder;
                template <class Info>
                Info* find(auto& tb, Info*& cache, StreamID id) {
                    if (cache && cache->st.id == id) {
                        return cache;
                    }
                    if (auto found = tb.find(id.id); found != tb.end()) {
                        cache = std::addressof(found->second);
                        return cache;
                    }
                    return nullptr;
                }

                BidiInfo* find_bidi_local(I& h, StreamID id) {
                    return find<BidiInfo>(h.bidi_local, h.bl_cache, id);
                }

                BidiInfo* find_bidi_peer(I& h, StreamID id) {
                    return find<BidiInfo>(h.bidi_peer, h.bp_cache, id);
                }

                BidiInfo* find_bidi(I& h, StreamID id) {
                    if (id.dir() == h.self_dir) {
                        return find_bidi_local(h, id);
                    }
                    else {
                        return find_bidi_peer(h, id);
                    }
                }

                UniSendInfo* find_uni_send(I& h, StreamID id) {
                    return find<UniSendInfo>(h.uni_send, h.us_cache, id);
                }

                UniRecvInfo* find_uni_recv(I& h, StreamID id) {
                    return find<UniRecvInfo>(h.uni_recv, h.ur_cache, id);
                }

                error::Error on_ack(I& h, size_t packet_number) {
                    auto found = h.frags.find(packet_number);
                    if (found == h.frags.end()) {
                        return error::none;  // no stream found
                    }
                    for (auto& frag : found->second.frags) {
                        frag->ack = true;
                    }
                    h.frags.erase(packet_number);
                    return error::none;
                }

                error::Error on_lost(I& h, size_t packet_number) {
                    auto found = h.frags.find(packet_number);
                    if (found == h.frags.end()) {
                        return error::none;  // no stream found
                    }
                    for (auto& frag : found->second.frags) {
                        frag->lost = true;
                    }
                    h.frags.erase(packet_number);
                    return error::none;
                }

                error::Error on_send_packet(I& h, StreamID id, StreamFragment frag) {
                    auto send_frag = [&](auto buf) -> error::Error {
                        if (!buf) {
                            return QUICError{
                                .msg = "no stream found at sending packet",
                            };
                        }
                        auto val = buf->sbuf.send_packet(frag);
                        if (!val) {
                            return error::memory_exhausted;
                        }
                        h.frags[frag.packet_number].frags.push_back(std::move(val));
                        return error::none;
                    };
                    if (id.type() == StreamType::bidi) {
                        return send_frag(find_bidi(h, id));
                    }
                    else if (id.type() == StreamType::uni) {
                        if (id.dir() == h.self_dir) {
                            return send_frag(find_uni_send(h, id));
                        }
                        else {
                            return QUICError{
                                .msg = "cannot send STREAM frame at unidirectional receipt stream. library bug!!",
                            };
                        }
                    }
                    return QUICError{
                        .msg = "invalid stream id. library bug!!",
                    };
                }

               private:
                template <class Info>
                error::Error create(StreamID id, auto& tb, Info*& cache) {
                    auto dup = tb.emplace(id, Info{.st = {.id = id}});
                    if (!dup.second) {
                        return QUICError{
                            .msg = "duplicated creation. library bug!!",
                        };
                    }
                    cache = std::addressof(dup.first->second);
                    return error::none;
                }

               public:
                error::Error create_stream(I& h, StreamID id, OpenReason reason) {
                    if (id.type() == StreamType::bidi) {
                        if (id.dir() == h.self_dir) {
                            return create<BidiInfo>(id, h.bidi_local, h.bl_cache);
                        }
                        else {
                            return create<BidiInfo>(id, h.bidi_peer, h.bp_cache);
                        }
                    }
                    else if (id.type() == StreamType::uni) {
                        if (id.dir() == h.self_dir) {
                            return create<UniSendInfo>(id, h.uni_send, h.us_cache);
                        }
                        else {
                            return create<UniRecvInfo>(id, h.uni_recv, h.ur_cache);
                        }
                    }
                    return QUICError{
                        .msg = "invalid stream id. library bug!!",
                    };
                }

                RecvUniStreamState* recv_state(I& h, StreamID id) {
                    if (id.type() == StreamType::bidi) {
                        auto found = find_bidi(h, id);
                        return found ? &found->st.recv : nullptr;
                    }
                    else {
                        if (id.dir() == h.self_dir) {
                            return nullptr;
                        }
                        else {
                            auto found = find_uni_recv(h, id);
                            return found ? &found->st.state : nullptr;
                        }
                    }
                }

                SendUniStreamState* send_state(I& h, StreamID id) {
                    if (id.type() == StreamType::bidi) {
                        auto found = find_bidi(h, id);
                        return found ? &found->st.send : nullptr;
                    }
                    else {
                        if (id.dir() == h.self_dir) {
                            auto found = find_uni_send(h, id);
                            return found ? &found->st.state : nullptr;
                        }
                        else {
                            return nullptr;
                        }
                    }
                }

                error::Error recv_stream(I& h, StreamID id, size_t offset, ByteLen data) {
                    auto known = [&](auto* found) -> error::Error {
                        if (!found) {
                            return QUICError{
                                .msg = "stream not found. library bug!!",
                            };
                        }
                        return found->rbuf.recv(offset, data);
                    };
                    if (id.type() == StreamType::bidi) {
                        return known(find_bidi(h, id));
                    }
                    else {
                        return known(find_uni_recv(h, id));
                    }
                }

                bool get_new_stream_data(I& h, StreamID id, StreamBuf& buf) {
                    auto alloc = [&](auto* found) {
                        if (!found) {
                            return false;
                        }
                        return found->sbuf.allocate_fragment(buf);
                    };
                    if (id.type() == StreamType::bidi) {
                        return alloc(find_bidi(h, id));
                    }
                    else {
                        return alloc(find_uni_send(h, id));
                    }
                }

                error::Error stream_fragment(I& h, StreamID id, StreamFragment frag) {
                    auto decide = [&](auto* found) -> error::Error {
                        if (!found) {
                            return QUICError{
                                .msg = "stream not found",
                            };
                        }
                        found->sbuf.decide_fragment(frag);
                        return error::none;
                    };
                    if (id.type() == StreamType::bidi) {
                        return decide(find_bidi(h, id));
                    }
                    else {
                        return decide(find_uni_send(h, id));
                    }
                }

                bool get_retransmit(I& h, StreamID id, StreamFragment& frag) {
                    auto get = [&](auto* found) {
                        if (!found) {
                            return false;
                        }
                        return found->sbuf.allocate_retransmit_fragment(frag);
                    };
                    if (id.type() == StreamType::bidi) {
                        return get(find_bidi(h, id));
                    }
                    else {
                        return get(find_uni_send(h, id));
                    }
                }

                error::Error submit_retransmit(I& h, StreamID id, StreamFragment frag) {
                    auto submit = [&](auto* found) -> error::Error {
                        if (!found) {
                            return QUICError{
                                .msg = "stream not found",
                            };
                        }
                        found->sbuf.retransmit_fragment(frag);
                        return error::none;
                    };
                    if (id.type() == StreamType::bidi) {
                        return submit(find_bidi(h, id));
                    }
                    else {
                        return submit(find_uni_send(h, id));
                    }
                }
            };
        }  // namespace quic::stream::impl
    }      // namespace dnet
}  // namespace utils
