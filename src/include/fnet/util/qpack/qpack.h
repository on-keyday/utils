/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "encoder.h"
#include "decoder.h"
#include "field_line.h"
#include <wrap/light/enum.h>
#include "../http/header.h"

namespace utils {
    namespace qpack {
        enum class FieldPolicy {
            prior_static = 0x01,
            prior_dynamic = 0x02,
            prior_index = 0x03,
            force_literal = 0x04,
            no_dynamic = 0x5,

            prior_mask = 0xff,

            maybe_insert = 0x100,
            must_forward_as_literal = 0x200,
        };

        DEFINE_ENUM_FLAGOP(FieldPolicy);

        template <class String>
        struct DecodeField {
            String head;
            String value;
            bool forward_as_literal = false;

            void set_header(auto&& h) {
                encoder::assign_to(head, std::forward<decltype(h)>(h));
            }

            void set_value(auto&& v) {
                encoder::assign_to(value, std::forward<decltype(v)>(v));
            }

            void set_forward_mode(bool as_literal) {
                forward_as_literal = as_literal;
            }

            String get_string() const {
                return String{};
            }
        };

        template <class TypeConfig>
        struct Context {
            fields::FieldEncodeContext<TypeConfig> enc;
            fields::FieldDecodeContext<TypeConfig> dec;
            using String = typename TypeConfig::string;
            binary::expand_writer<String> enc_stream;
            binary::expand_writer<String> dec_stream;
            std::uint64_t expires_delta = 0;

            QpackError read_encoder_stream(binary::reader& r) {
                while (true) {
                    auto base = r.offset();
                    auto err = encoder::parse_instruction(dec, r);
                    if (err == QpackError::input_length) {
                        r.reset(base);
                        return QpackError::none;
                    }
                    if (err != QpackError::none) {
                        return err;
                    }
                }
            }

            bool is_expired(std::uint64_t index) const {
                return index - enc.get_dropped() < expires_delta;
            }

            auto drop_until_add(auto&& key, auto&& value, auto&& stop_drop, auto&& on_added) {
                while (true) {
                    if (stop_drop()) {
                        break;
                    }
                    auto new_ref = enc.add_field(key, value);
                    if (new_ref != fields::no_ref) {
                        return on_added(new_ref);
                    }
                    auto err = enc.drop_field();
                    if (err != QpackError::none &&
                        err != QpackError::dynamic_ref_exists &&
                        err != QpackError::no_entry_exists) {
                        return err;
                    }
                    if (err == QpackError::dynamic_ref_exists || err == QpackError::no_entry_exists) {
                        break;
                    }
                }
                return QpackError::dynamic_ref_exists;
            }

            QpackError update_capacity(std::uint64_t capa) {
                if (auto err = enc.update_capacity(capa); err != QpackError::none) {
                    return err;
                }
                return encoder::render_capacity(enc_stream, capa);
            }

            QpackError add_entry(auto&& head, auto&& value) {
                fields::Reference s_refs = fields::find_static_ref(head, value);
                if (s_refs.head_value != fields::no_ref) {
                    // no need to add
                    // because static table has this entry
                    return QpackError::none;
                }
                fields::Reference d_refs = enc.find_ref(head, value);
                if (d_refs.head_value != fields::no_ref) {
                    auto err = drop_until_add(
                        head, value,
                        [&] {
                            // decoder cannot refer d_refs.head_value field
                            // because that entries are already dropped
                            // when enc.get_dropped() == d_refs.head_value,
                            // we can duplicate it before dropping
                            return enc.get_dropped() > d_refs.head_value;
                        },
                        [&](std::uint64_t new_ref) {
                            return encoder::render_duplicate(enc_stream, d_refs.head_value);
                        });
                    if (err != QpackError::dynamic_ref_exists) {
                        return err;
                    }
                }
                if (s_refs.head != fields::no_ref) {
                    auto err = drop_until_add(
                        head, value,
                        [&] {
                            return false;
                        },
                        [&](std::uint64_t new_ref) {
                            return encoder::render_insert_index_literal(enc_stream, s_refs.head, true, value);
                        });
                    if (err != QpackError::dynamic_ref_exists) {
                        return err;
                    }
                }
                if (d_refs.head != fields::no_ref) {
                    auto err = drop_until_add(
                        head, value,
                        [&] {
                            // decoder cannot refer d_refs.head field
                            // because that entries are already dropped
                            // when enc.get_dropped() == d_refs.head_value,
                            // we can duplicate it before dropping
                            return enc.get_dropped() > d_refs.head;
                        },
                        [&](std::uint64_t new_ref) {
                            return encoder::render_insert_index_literal(enc_stream, d_refs.head_value, false, value);
                        });
                    if (err != QpackError::dynamic_ref_exists) {
                        return err;
                    }
                }
                return drop_until_add(
                    head, value,
                    [] { return false; },
                    [&](std::uint64_t new_ref) {
                        return encoder::render_insert_literal(enc_stream, head, value);
                    });
            }

            template <class T>
            QpackError write_key_value(StreamID id, binary::expand_writer<T>& w, const fields::SectionPrefix& prefix, auto&& key, auto&& value, FieldPolicy policy, std::uint64_t* max_ref) {
                fields::Reference s_refs;
                fields::Reference d_refs;
                auto set_max_ref = [&](std::uint64_t ref) {
                    if (*max_ref == fields::no_ref || *max_ref < ref) {
                        *max_ref = ref;
                    }
                };
                auto write_static = [&] {
                    s_refs = fields::find_static_ref(key, value);
                    if (s_refs.head_value != fields::no_ref) {
                        return fields::render_field_line_index(w, s_refs.head_value, true);
                    }
                    if (s_refs.head != fields::no_ref) {
                        return fields::render_field_line_index_literal(w, s_refs.head, true, value, any(policy & FieldPolicy::must_forward_as_literal));
                    }
                    return QpackError::no_entry_exists;
                };
                auto write_dynamic = [&] {
                    d_refs = enc.find_ref(key, value);
                    if (d_refs.head_value != fields::no_ref &&
                        !is_expired(d_refs.head_value)) {
                        if (auto err = fields::render_field_line_dynamic_index(prefix, w, d_refs.head_value); err != QpackError::none) {
                            return err;
                        }
                        set_max_ref(d_refs.head_value);
                        return enc.add_ref(d_refs.head_value, id);
                    }
                    if (d_refs.head != fields::no_ref &&
                        !is_expired(d_refs.head)) {
                        if (auto err = fields::render_field_line_dynamic_index_literal(prefix, w, d_refs.head, value, any(policy & FieldPolicy::must_forward_as_literal)); err != QpackError::none) {
                            return err;
                        }
                        set_max_ref(d_refs.head);
                        return enc.add_ref(d_refs.head, id);
                    }
                    return QpackError::no_entry_exists;
                };
                auto proior = FieldPolicy::prior_mask & policy;
                switch (proior) {
                    default:
                        return QpackError::undefined_instruction;
                    case FieldPolicy::prior_static: {
                        auto err = write_static();
                        if (err != QpackError::no_entry_exists) {
                            return err;
                        }
                        err = write_dynamic();
                        if (err != QpackError::no_entry_exists) {
                            return err;
                        }
                        break;
                    }
                    case FieldPolicy::prior_dynamic: {
                        auto err = write_dynamic();
                        if (err != QpackError::no_entry_exists) {
                            return err;
                        }
                        err = write_static();
                        if (err != QpackError::no_entry_exists) {
                            return err;
                        }
                        break;
                    }
                    case FieldPolicy::prior_index: {
                        s_refs = fields::find_static_ref(key, value);
                        if (s_refs.head_value != fields::no_ref) {
                            return fields::render_field_line_index(w, s_refs.head_value, true);
                        }
                        d_refs = enc.find_ref(key, value);
                        if (d_refs.head_value != fields::no_ref &&
                            !is_expired(d_refs.head_value)) {
                            if (auto err = fields::render_field_line_dynamic_index(prefix, w, d_refs.head_value); err != QpackError::none) {
                                return err;
                            }
                            set_max_ref(d_refs.head_value);
                            return enc.add_ref(d_refs.head_value, id);
                        }
                        if (s_refs.head != fields::no_ref) {
                            return fields::render_field_line_index_literal(w, s_refs.head, true, value, any(policy & FieldPolicy::must_forward_as_literal));
                        }
                        if (d_refs.head != fields::no_ref &&
                            !is_expired(d_refs.head)) {
                            if (auto err = fields::render_field_line_dynamic_index_literal(prefix, w, d_refs.head, value, any(policy & FieldPolicy::must_forward_as_literal));
                                err != QpackError::none) {
                                return err;
                            }
                            set_max_ref(d_refs.head);
                            return enc.add_ref(d_refs.head, id);
                        }
                        break;
                    }
                    case FieldPolicy::force_literal: {
                        return fields::render_field_line_literal(w, key, value, any(policy & FieldPolicy::must_forward_as_literal));
                    }
                    case FieldPolicy::no_dynamic: {
                        auto err = write_static();
                        if (err != QpackError::no_entry_exists) {
                            return err;
                        }
                        return fields::render_field_line_literal(w, key, value, any(policy & FieldPolicy::must_forward_as_literal));
                    }
                }
                if (!any(policy & FieldPolicy::maybe_insert)) {
                    return fields::render_field_line_literal(w, key, value, any(policy & FieldPolicy::must_forward_as_literal));
                }

                // find chance to duplicate
                if (d_refs.head_value != fields::no_ref) {
                    auto err = drop_until_add(
                        key, value,
                        [&] {
                            // decoder cannot refer d_refs.head_value field
                            // because that entries are already dropped
                            // when enc.get_dropped() == d_refs.head_value,
                            // we can duplicate it before dropping
                            return enc.get_dropped() > d_refs.head_value;
                        },
                        [&](std::uint64_t new_ref) {
                            if (auto err = encoder::render_duplicate(enc_stream, d_refs.head_value); err != QpackError::none) {
                                return err;
                            }
                            if (auto err = fields::render_field_line_dynamic_index(prefix, w, new_ref); err != QpackError::none) {
                                return err;
                            }
                            set_max_ref(new_ref);
                            return enc.add_ref(new_ref, id);
                        });
                    if (err != QpackError::dynamic_ref_exists) {
                        return err;
                    }
                }
                // find chance to refer index insertion
                if (d_refs.head != fields::no_ref) {
                    auto err = drop_until_add(
                        key, value,
                        [&] {
                            // decoder cannot refer d_refs.head field
                            // because that entries are already dropped
                            // when enc.get_dropped() == d_refs.head_value,
                            // we can duplicate it before dropping
                            return enc.get_dropped() > d_refs.head;
                        },
                        [&](std::uint64_t new_ref) {
                            if (auto err = encoder::render_insert_index_literal(enc_stream, d_refs.head_value, false, value); err != QpackError::none) {
                                return err;
                            }
                            if (auto err = fields::render_field_line_dynamic_index(prefix, w, new_ref); err != QpackError::none) {
                                return err;
                            }
                            set_max_ref(new_ref);
                            return enc.add_ref(new_ref, id);
                        });
                    if (err != QpackError::dynamic_ref_exists) {
                        return err;
                    }
                }
                // try inserting with literal
                auto err = drop_until_add(
                    key, value,
                    [] { return false; },
                    [&](std::uint64_t new_ref) {
                        if (auto err = encoder::render_insert_literal(enc_stream, key, value); err != QpackError::none) {
                            return err;
                        }
                        if (auto err = fields::render_field_line_dynamic_index(prefix, w, new_ref); err != QpackError::none) {
                            return err;
                        }
                        set_max_ref(new_ref);
                        return enc.add_ref(new_ref, id);
                    });
                if (err != QpackError::dynamic_ref_exists) {
                    return err;
                }
                // fallback to literal field line
                return fields::render_field_line_literal(w, key, value, any(policy & FieldPolicy::must_forward_as_literal));
            }

            // write should be void(add_entry,add_field)
            // add_entry is bool(header,value)
            // add_field is bool(header,value,FieldPolicy policy=FieldPolicy::proior_static)
            template <class T, class H>
            QpackError write_header(StreamID id, binary::expand_writer<T>& w, H&& write) {
                fields::SectionPrefix prefix;
                prefix.base = enc.get_inserted();
                binary::expand_writer<typename TypeConfig::string> tw;
                QpackError err = QpackError::none;
                std::uint64_t max_ref = fields::no_ref;
                auto add_field = [&](auto&& head, auto&& value, FieldPolicy policy = FieldPolicy::prior_static) {
                    if (err != QpackError::none) {
                        return false;
                    }
                    err = write_key_value(id, tw, prefix, head, value, policy, &max_ref);
                    return err == QpackError::none;
                };
                auto add_encoder_entry = [&](auto&& head, auto&& value) {
                    if (err != QpackError::none) {
                        return false;
                    }
                    err = add_entry(head, value);
                    bool inserted = err == QpackError::none;
                    if (err == QpackError::dynamic_ref_exists) {
                        err = QpackError::none;
                    }
                    return inserted;
                };
                write(add_encoder_entry, add_field);
                if (err != QpackError::none) {
                    return err;
                }
                if (tw.written().size() == 0) {
                    return QpackError::output_length;
                }
                if (max_ref == fields::no_ref) {
                    prefix.base = 0;
                    prefix.required_insert_count = 0;
                }
                else {
                    prefix.required_insert_count = max_ref + 1;
                }
                fields::EncodedSectionPrefix enc_prefix;
                enc_prefix.encode(prefix, enc.get_max_capacity());
                err = enc_prefix.render(w);
                if (err != QpackError::none) {
                    return err;
                }
                if (!w.write(tw.written())) {
                    return QpackError::output_length;
                }
                return QpackError::none;
            }

            QpackError read_header(StreamID id, binary::reader& r, auto&& read) {
                fields::EncodedSectionPrefix enc_prefix;
                if (auto err = enc_prefix.parse(r); err != QpackError::none) {
                    return err;
                }
                fields::SectionPrefix prefix;
                if (auto err = enc_prefix.decode(prefix, dec.get_inserted(), dec.get_max_capacity());
                    err != QpackError::none) {
                    return err;
                }
                if (prefix.required_insert_count > dec.get_inserted()) {
                    return QpackError::head_of_blocking;
                }
                DecodeField<String> field;
                while (!r.empty()) {
                    if (auto err = fields::parse_field_line(dec, prefix, r, field);
                        err != QpackError::none) {
                        return err;
                    }
                    read(field);
                    field = {};
                }
                return decoder::render_instruction(dec, dec_stream, decoder::Instruction::section_ack, id);
            }
        };

        auto http3_field_validate_wrapper(auto& field) {
            return [&](auto&& h, auto&& v, FieldPolicy policy = FieldPolicy::prior_static) {
                if (!http::header::http2_validator(true, true)(h, v)) {
                    return false;
                }
                return field(h, v, policy);
            };
        }

        auto http3_entry_validate_wrapper(auto& entry) {
            return [&](auto&& h, auto&& v) {
                if (!http::header::http2_validator(true, true)(h, v)) {
                    return false;
                }
                return entry(h, v);
            };
        }
    }  // namespace qpack
}  // namespace utils
