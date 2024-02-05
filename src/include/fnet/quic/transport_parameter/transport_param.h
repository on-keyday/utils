/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// transport_param - transport param format
#pragma once
#include "../varint.h"
#include "../../../binary/number.h"
#include "defined_id.h"

namespace futils {
    namespace fnet::quic::trsparam {
        struct TransportParamValue {
           private:
            bool is_varint = false;
            union {
                view::rvec byts;
                varint::Value qvint;
            };

           public:
            constexpr TransportParamValue(const TransportParamValue&) = default;
            constexpr TransportParamValue& operator=(const TransportParamValue&) = default;

            constexpr TransportParamValue()
                : is_varint(false), byts(view::rvec{}) {}

            constexpr TransportParamValue(view::rvec v)
                : is_varint(false), byts(v) {}

            constexpr TransportParamValue(size_t v)
                : is_varint(true), qvint(v) {}

            constexpr bool parse(binary::reader& r, size_t len) {
                return r.read(byts, len);
            }

            constexpr size_t len() const {
                if (is_varint) {
                    return varint::len(qvint);
                }
                else {
                    return byts.size();
                }
            }

            constexpr bool render(binary::writer& w) const {
                if (is_varint) {
                    return varint::write(w, qvint);
                }
                else {
                    return w.write(byts);
                }
            }

            constexpr bool as_qvarint() {
                if (is_varint) {
                    return true;
                }
                binary::reader r{byts};
                auto [val, ok] = varint::read(r);
                if (ok && r.empty()) {
                    qvint = val;
                    is_varint = true;
                    return true;
                }
                return false;
            }

            constexpr bool as_bytes() {
                return !is_varint;
            }

            varint::Value qvarint() const {
                if (!is_varint) {
                    return varint::Value{~std::uint64_t(0)};
                }
                return qvint;
            }

            view::rvec bytes() const {
                if (is_varint) {
                    return {};
                }
                return byts;
            }
        };

        struct TransportParamID {
            varint::Value id;

            constexpr TransportParamID() = default;
            constexpr TransportParamID(varint::Value id)
                : id(id) {}

            constexpr TransportParamID(DefinedID id)
                : id(std::uint64_t(id)) {}

            constexpr operator std::uint64_t() const {
                return id;
            }

            constexpr operator DefinedID() const {
                return DefinedID(id.value);
            }
        };

        struct TransportParameter {
            TransportParamID id;
            varint::Value length;  // only parse
            TransportParamValue data;

            constexpr bool parse(binary::reader& r) {
                return varint::read(r, id.id) &&
                       varint::read(r, length) &&
                       data.parse(r, length);
            }

            constexpr size_t len(bool use_length_field = false) const {
                return varint::len(id.id) +
                       (use_length_field ? varint::len(length) : varint::len(data.len())) +
                       data.len();
            }

            constexpr bool render(binary::writer& w) const {
                return varint::write(w, id.id) &&
                       varint::write(w, data.len()) &&
                       data.render(w);
            }
        };

        constexpr TransportParameter make_transport_param(TransportParamID id, view::rvec data) {
            return {id, size_t(), data};
        }

        constexpr TransportParameter make_transport_param(TransportParamID id, std::uint64_t data) {
            return {id, size_t(), data};
        }

    }  // namespace fnet::quic::trsparam
}  // namespace futils
