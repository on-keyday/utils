/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// field_refs - QPACK encoder field reference
#pragma once
#include "../../fnet/quic/stream/stream_id.h"
#include "../../fnet/quic/hash_fn.h"
#include "error.h"

namespace utils {
    namespace qpack {
        using StreamID = fnet::quic::stream::StreamID;
        namespace fields {
            struct InsertDropManager {
               private:
                std::uint64_t inserted = 0;
                std::uint64_t dropped = 0;

               public:
                constexpr std::uint64_t get_relative_range() const {
                    return inserted - dropped;
                }

                constexpr bool is_valid_relatvie_index(std::uint64_t index) const {
                    const auto range_max = get_relative_range();
                    return index < range_max;
                }

                constexpr std::pair<std::uint64_t, bool> abs_to_rel(std::uint64_t index) const {
                    if (index < dropped || inserted <= index) {
                        return {-1, false};
                    }
                    return {inserted - 1 - index, true};
                }

                constexpr void insert(size_t in = 1) {
                    inserted += in;
                }

                constexpr void drop(size_t in = 1) {
                    dropped += in;
                }

                constexpr std::uint64_t get_inserted() const {
                    return inserted;
                }

                constexpr std::uint64_t get_dropped() const {
                    return dropped;
                }
            };

            constexpr std::uint64_t calc_field_usage(auto&& head, auto&& value) {
                return head.size() + value.size() + 32;
            }

            constexpr std::uint64_t no_ref = ~0;

            struct Reference {
                std::uint64_t head = no_ref;
                std::uint64_t head_value = no_ref;
            };

            // Track is tracked values by encoder/decoder
            struct Track {
                InsertDropManager isdr;
                std::uint64_t known_insert_count = 0;

                std::uint64_t max_capacity = 0;
                std::uint64_t capacity = 0;
                std::uint64_t usage = 0;
            };

            template <class TypeConfig>
            struct FieldEncodeContext {
               private:
                template <class K, class V>
                using RefMap = typename TypeConfig::template ref_map<K, V>;
                template <class V>
                using Vec = typename TypeConfig::template vec<T>;
                template <class V>
                using FieldQue = typename TypeConfig::template field_que<V>;
                using String = typename TypeConfig::string;

                RefMap<StreamID, Vec<std::uint64_t>> id_ref;
                RefMap<std::uint64_t, std::uint64_t> ref_count;

                FieldQue<std::pair<String, String>> enc_table;
                Track track;

               public:
                constexpr void set_max_capacity(std::uint64_t capa) {
                    track.max_capacity = capa;
                }

                constexpr std::uint64_t get_inserted() const {
                    return track.isdr.get_inserted();
                }
                constexpr std::uint64_t get_max_capacity() const {
                    return track.max_capacity;
                }

                constexpr QpackError update_capacity(std::uint64_t capa) {
                    if (capa > track.max_capacity) {
                        return QpackError::large_capacity;
                    }
                    if (capa < track.usage) {
                        return QpackError::small_capacity;
                    }
                    track.capacity = capa;
                    return QpackError::none;
                }

                constexpr bool can_add_field(auto&& head, auto&& value) const {
                    return track.usage + calc_usage(head, value) <= track.capacity;
                }

                // returns entrie index
                // if insertion is blocked (HOL-blocking),
                // returns no_ref
                constexpr std::uint64_t add_field(auto&& head, auto&& value) {
                    auto add = calc_usage(head, value);
                    if (track.usage + add <= track.capacity) {
                        return no_ref;
                    }
                    enc_que.push_back(std::pair<String, String>{
                        std::forward<decltype(head)>(head),
                        std::forward<decltype(value)>(value),
                    });
                    track.usage += add;
                    track.isdr.insert(1);
                    return track.isdr.get_inserted() - 1;
                }

                constexpr Reference find_ref(auto&& head, auto&& value) const {
                    Reference ref;
                    size_t i = 0;
                    auto max_index = track.isdr.get_inserted() - 1;
                    for (auto it = enc_table.rbegin(); it != enc_table.rend(); it++) {
                        auto& hd = std::get<0>(*it);
                        auto& val = std::get<1>(*it);
                        if (hd == head) {
                            if (ref.head != no_ref) {
                                ref.head = max_index - i;
                            }
                            if (val == value) {
                                ref.head_value = max_index - i;
                                // no need to search more
                                return ref;
                            }
                        }
                        i++;
                    }
                    return ref;
                }

                // add_ref adds reference to entries
                // this is called when index `ref` is used for field line representation
                constexpr QpackError add_ref(std::uint64_t ref, StreamID id) {
                    if (ref < track.isdr.get_dropped() || track.isdr.get_inserted() <= ref) {
                        return QpackError::invalid_dynamic_table_abs_ref;
                    }
                    ref_count[ref]++;
                    id_ref[id].push_back(ref);
                    return QpackError::none;
                }

                // remove_ref removes reference to entries
                // this is called when section_ack or stream_cancel instruction is received
                constexpr QpackError remove_ref(StreamID id, bool is_ack) {
                    auto found = id_ref.find(id);
                    if (found == id_ref.end()) {
                        return QpackError::invalid_stream_id;
                    }
                    Vec<std::uint64_t>& acked = std::get<1>(*found);
                    for (auto& ref : acked) {
                        if (ref_count[ref] == 0) {
                            return QpackError::invalid_stream_id;
                        }
                        ref_count[ref]--;
                        if (is_ack) {
                            // ref is index value
                            // known_insert_count is size value
                            // |0|1|2|
                            // insert_count = 3
                            // known_insert_count = 2
                            // when ref = 2
                            // known_insert_count should become 3
                            if (ref + 1 > track.known_insert_count) {
                                track.known_insert_count = ref + 1;
                            }
                        }
                    }
                    id_ref.erase(found);
                    return QpackError::none;
                }

                // increment increments known_insert_count
                // this is called when increment instruction is received
                constexpr QpackError increment(std::uint64_t incr) {
                    if (track.known_insert_count + incr > track.isdr.get_inserted()) {
                        return QpackError::invalid_increment;
                    }
                    track.known_insert_count += incr;
                    return QpackError::none;
                }

                // drop_field drops
                constexpr QpackError drop_field() {
                    if (track.isdr.get_inserted() - track.isdr.get_dropped() == 0) {
                        return QpackError::no_entry_exists;
                    }
                    auto to_drop = track.isdr.get_dropped();
                    if (to_drop <= track.known_insert_count) {
                        return QpackError::dynamic_ref_exists;
                    }
                    if (ref_count[to_drop] != 0) {
                        return QpackError::dynamic_ref_exists;
                    }
                    auto& rem = enc_table.front();
                    track.usage -= calc_usage(get<0>(rem), get<1>(rem));
                    enc_table.pop_front();
                    ref_count.erase(to_drop);
                    track.isdr.drop(1);
                    return QpackError::none;
                }
            };

            template <class TypeConfig>
            struct FieldDecodeContext {
               private:
                template <class V>
                using FieldQue = typename TypeConfig::template field_que<V>;
                using String = typename TypeConfig::string;

                FieldQue<std::pair<String, String>> dec_que;

                Track track;

               public:
                constexpr void set_max_capacity(std::uint64_t capa) {
                    track.max_capacity = capa;
                }

                constexpr std::uint64_t get_max_capacity() const {
                    return track.max_capacity;
                }

                constexpr bool update_capacity(std::uint64_t capa) {
                    if (capa > track.max_capacity) {
                        return false;
                    }
                    track.max_capacity = capa;
                    return true;
                }

                constexpr QpackError add_field(auto&& head, auto&& value) {
                    auto field_usage = calc_field_usage(head, value);
                    if (field_usage > track.capacity) {
                        return QpackError::large_field;
                    }
                    dec_que.push_back(std::pair<String, String>{
                        std::forward<decltype(head)>(head),
                        std::forward<decltype(value)>(value),
                    });
                    track.usage += field_usage;
                    isdr.insert(1);
                    while (track.usage > track.capacity) {
                        assert(dec_que.size());
                        auto& field = dec_que.front();
                        auto fu = calc_field_usage(get<0>(field), get<1>(field));
                        assert(track.usage >= fu);
                        track.usage -= fu;
                        dec_que.pop_front();
                        track.isdr.drop(1);
                    }
                    return QpackError::none;
                }

                constexpr auto to_abs(std::uint64_t rel) {
                    return rel + track.isdr.get_inserted() - 1;
                }

                constexpr const std::pair<String, String>* find_ref(std::uint64_t abs_index) const {
                    auto [index, ok] = track.isdr.abs_to_rel(abs_index);
                    if (!ok) {
                        return nullptr;
                    }
                    assert(index < dec_que.size());
                    return &dec_que[index];
                }

                constexpr std::uint64_t increment_delta() const {
                    return track.isdr.get_inserted() - track.known_insert_count;
                }

                constexpr void increment_sent() {
                    track.known_insert_count = track.isdr.get_inserted();
                }

                constexpr bool section_ack_sent(std::uint64_t max_index) {
                    if (max_index >= track.isdr.get_inserted()) {
                        return false;
                    }
                    if (track.known_insert_count < max_index + 1) {
                        track.known_insert_count = max_index + 1;
                    }
                    return true;
                }
            };
        }  // namespace fields
    }      // namespace qpack
}  // namespace utils
