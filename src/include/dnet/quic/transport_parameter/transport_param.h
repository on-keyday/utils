/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// transport_param - transport parameter
#pragma once
#include <cstdint>
#include "../../std/array.h"
#include "../../../view/iovec.h"
#include "../varint.h"
#include "../../../io/number.h"
#include "../../storage.h"
#include "defined_id.h"
#include "../stream/initial_limits.h"

namespace utils {
    namespace dnet::quic::trsparam {

        struct PreferredAddress {
            byte ipv4_address[4]{};
            std::uint16_t ipv4_port = 0;
            byte ipv6_address[16]{};
            std::uint16_t ipv6_port{};
            byte connectionID_length;  // only parse
            view::rvec connectionID;
            byte stateless_reset_token[16];

            constexpr bool parse(io::reader& r) {
                return r.read(ipv4_address) &&
                       io::read_num(r, ipv4_port, true) &&
                       r.read(ipv6_address) &&
                       io::read_num(r, ipv6_port, true) &&
                       r.read(view::wvec(&connectionID_length, 1)) &&
                       r.read(connectionID, connectionID_length) &&
                       r.read(stateless_reset_token);
            }

            constexpr size_t len() const {
                return 4 + 2 + 16 + 2 +
                       1 +  // length
                       connectionID.size() + 16;
            }

            constexpr bool render(io::writer& w) const {
                if (connectionID.size() > 0xff) {
                    return false;
                }
                return w.write(ipv4_address) &&
                       io::write_num(w, ipv4_port, true) &&
                       w.write(ipv6_address) &&
                       io::write_num(w, ipv6_port, true) &&
                       w.write(byte(connectionID.size()), 1) &&
                       w.write(connectionID) &&
                       w.write(stateless_reset_token);
            }
        };

        using enough_to_store_preferred_address =
            byte[4 + 2 + 16 + 2 + 1 + 20 + 16];

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
                : byts(view::rvec{}), is_varint(false) {}

            constexpr TransportParamValue(view::rvec v)
                : byts(v), is_varint(false) {}

            constexpr TransportParamValue(size_t v)
                : qvint(v), is_varint(true) {}

            constexpr bool parse(io::reader& r, size_t len) {
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

            constexpr bool render(io::writer& w) const {
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
                io::reader r{byts};
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

            constexpr bool parse(io::reader& r) {
                return varint::read(r, id.id) &&
                       varint::read(r, length) &&
                       data.parse(r, length);
            }

            constexpr size_t len(bool use_length_field = false) const {
                return varint::len(id.id) +
                       (use_length_field ? varint::len(length) : varint::len(data.len())) +
                       data.len();
            }

            constexpr bool render(io::writer& w) const {
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

        struct DuplicateSetChecker {
            bool check[implemented_defined] = {};
            bool detect = false;
        };

        constexpr bool is_default_value(TransportParameter param) {
            if (param.id == DefinedID::max_udp_payload_size) {
                return param.data.qvarint() == 65527;
            }
            if (param.id == DefinedID::ack_delay_exponent) {
                return param.data.qvarint() == 3;
            }
            if (param.id == DefinedID::max_ack_delay) {
                return param.data.qvarint() == 25;
            }
            if (param.id == DefinedID::active_connection_id_limit) {
                return param.data.qvarint() == 2;
            }
            if (param.id == DefinedID::max_idle_timeout ||
                param.id == DefinedID::initial_max_data ||
                param.id == DefinedID::initial_max_stream_data_bidi_local ||
                param.id == DefinedID::initial_max_stream_data_bidi_remote ||
                param.id == DefinedID::initial_max_stream_data_uni ||
                param.id == DefinedID::initial_max_streams_bidi ||
                param.id == DefinedID::initial_max_streams_uni ||
                param.id == DefinedID::max_datagram_frame_size) {
                return param.data.qvarint() == 0;
            }
            return false;
        }

        struct DefinedTransportParams {
            view::rvec original_dst_connection_id;
            std::uint64_t max_idle_timeout = 0;
            byte stateless_reset_token[16];
            std::uint64_t max_udp_payload_size = 65527;
            std::uint64_t initial_max_data = 0;
            std::uint64_t initial_max_stream_data_bidi_local = 0;
            std::uint64_t initial_max_stream_data_bidi_remote = 0;
            std::uint64_t initial_max_stream_data_uni = 0;
            std::uint64_t initial_max_streams_bidi = 0;
            std::uint64_t initial_max_streams_uni = 0;
            std::uint64_t ack_delay_exponent = 3;
            std::uint64_t max_ack_delay = 25;
            bool disable_active_migration = false;
            PreferredAddress preferred_address;
            std::uint64_t active_connection_id_limit = 2;
            view::rvec initial_src_connection_id;
            view::rvec retry_src_connection_id;
            std::uint64_t max_datagram_frame_size = 0;
            bool grease_quic_bit = false;

            // returns(ok,changed)
            constexpr std::pair<bool, bool> from_transport_param(TransportParameter param, DuplicateSetChecker* checker = nullptr) {
                auto id = param.id;
                auto index = 0;
                auto make_ = [&](auto&& cb) {
                    return [&, cb](auto&& id) {
                        if (checker && checker->check[index]) {
                            checker->detect = true;
                            return false;
                        }
                        if (!cb(id)) {
                            return false;
                        }
                        if (checker) {
                            checker->check[index] = true;
                        }
                        return true;
                    };
                };
                auto vlint = make_([&](std::uint64_t& id) {
                    if (!param.data.as_qvarint()) {
                        return false;
                    }
                    id = param.data.qvarint();
                    return true;
                });
                auto rdata = make_([&](view::rvec& id) {
                    if (!param.data.as_bytes()) {
                        return false;
                    }
                    id = param.data.bytes();
                    return true;
                });
                auto wdata = make_([&](view::wvec id) {
                    if (!param.data.as_bytes()) {
                        return false;
                    }
                    return view::copy(id, param.data.bytes()) == 0;
                });
                auto bool_ = make_([&](bool& id) {
                    if (param.data.len() != 0) {
                        return false;
                    }
                    id = true;
                    return true;
                });
                bool suc = false;
                auto visit = [&](auto& cb, auto& val, DefinedID cmp) {
                    if (id == cmp) {
                        suc = cb(val);
                        return true;
                    }
                    index++;
                    return false;
                };
#define VLINT(ID)                          \
    if (visit(vlint, ID, DefinedID::ID)) { \
        return {suc, suc};                 \
    };
#define RDATA(ID)                          \
    if (visit(rdata, ID, DefinedID::ID)) { \
        return {suc, suc};                 \
    }
#define WDATA(ID)                          \
    if (visit(wdata, ID, DefinedID::ID)) { \
        return {suc, suc};                 \
    }
#define BOOL(ID)                           \
    if (visit(bool_, ID, DefinedID::ID)) { \
        return {suc, suc};                 \
    }

                RDATA(original_dst_connection_id);
                VLINT(max_idle_timeout);
                WDATA(stateless_reset_token);
                VLINT(max_udp_payload_size);
                VLINT(initial_max_data);
                VLINT(initial_max_stream_data_bidi_local);
                VLINT(initial_max_stream_data_bidi_remote);
                VLINT(initial_max_stream_data_uni);
                VLINT(initial_max_streams_bidi);
                VLINT(initial_max_streams_uni);
                VLINT(ack_delay_exponent);
                VLINT(max_ack_delay);
                BOOL(disable_active_migration)
                auto addr = make_([&](PreferredAddress& id) {
                    io::reader r{param.data.bytes()};
                    if (!id.parse(r) || !r.empty()) {
                        return false;
                    }
                    return true;
                });
                if (visit(addr, preferred_address, DefinedID::preferred_address)) {
                    return {suc, suc};
                }
                VLINT(active_connection_id_limit);
                RDATA(initial_src_connection_id);
                RDATA(retry_src_connection_id);
                VLINT(max_datagram_frame_size);
                BOOL(grease_quic_bit)
                return {true, false};  // not matched but no error
#undef RDATA
#undef WDATA
#undef VLINT
#undef BOOL
            }
            constexpr bool visit_each_param(auto&& visit) const {
                int index = 0;
                auto do_visit = [&](auto id, auto&& val) {
                    TransportParameter param = make_transport_param(id, val);
                    if (!visit(index, std::as_const(param), true)) {
                        return false;
                    }
                    index++;
                    return true;  // any way true
                };
#define MAKE(param)                           \
    if (!do_visit(DefinedID::param, param)) { \
        return false;                         \
    }

#define BOOL(param)                                                                   \
    if (!visit(index, make_transport_param(DefinedID::param, view::rvec{}), param)) { \
        return false;                                                                 \
    }                                                                                 \
    index++;

                MAKE(original_dst_connection_id);
                MAKE(max_idle_timeout);
                MAKE(stateless_reset_token);
                MAKE(max_udp_payload_size);
                MAKE(initial_max_data);
                MAKE(initial_max_stream_data_bidi_local);
                MAKE(initial_max_stream_data_bidi_remote);
                MAKE(initial_max_stream_data_uni);
                MAKE(initial_max_streams_bidi);
                MAKE(initial_max_streams_uni);
                MAKE(ack_delay_exponent);
                MAKE(max_ack_delay);
                BOOL(disable_active_migration);
                enough_to_store_preferred_address tmp;
                io::writer tmpw{tmp};
                preferred_address.render(tmpw);
                if (!visit(index, make_transport_param(DefinedID::preferred_address, view::rvec{tmpw.written()}), true)) {
                    return false;
                }
                index++;
                MAKE(active_connection_id_limit);
                MAKE(initial_src_connection_id);
                MAKE(retry_src_connection_id);
                MAKE(max_datagram_frame_size);
                BOOL(grease_quic_bit)
#undef MAKE
#undef BOOL
                return true;
            }

            template <class Array = slib::array<std::pair<TransportParameter, bool>, implemented_defined>>
            constexpr Array to_transport_param(view::wvec src) const {
                Array params{};
                visit_each_param([&](int index, TransportParameter param, bool exists) {
                    if (param.id == std::uint64_t(DefinedID::preferred_address)) {
                        auto data = param.data.bytes();
                        if (src.size() >= data.size()) {
                            auto p = src.substr(0, data.size());
                            view::copy(p, data);
                            param.data = p;
                            params[index] = {param, true};
                        }
                        else {
                            param.data = {};
                            params[index] = {param, false};
                        }
                        return true;
                    }
                    params[index] = {param, exists};
                });
                return params;
            }

            constexpr bool render(io::writer& w, auto&& should_render) const {
                return visit_each_param([&](int, TransportParameter param, bool exists) {
                    if (exists && should_render(std::as_const(param))) {
                        return param.render(w);
                    }
                    return true;
                });
            }
        };

        enum class TransportParamError {
            none,
            invalid_encoding,
            less_than_2_connection_id,
            less_than_1200_udp_payload,
            over_20_ack_delay_exponent,
            over_2pow14_max_ack_delay,
            zero_length_connid,
            too_large_max_streams_bidi,
            too_large_max_streams_uni,
        };

        constexpr const char* to_string(TransportParamError err) {
            switch (err) {
                case TransportParamError::invalid_encoding:
                    return "invalid transport parameter encoding";
                case TransportParamError::less_than_2_connection_id:
                    return "active_connection_id_limit is less than 2";
                case TransportParamError::less_than_1200_udp_payload:
                    return "max_udp_payload_size is less than 1200";
                case TransportParamError::over_20_ack_delay_exponent:
                    return "ack_delay_exponent is over 20";
                case TransportParamError::zero_length_connid:
                    return "preferred_address.connectionID.len is 0";
                case TransportParamError::too_large_max_streams_bidi:
                    return "max_bidi_streams greater than 2^60";
                case TransportParamError::too_large_max_streams_uni:
                    return "max_uni_streams greater than 2^60";
                default:
                    return nullptr;
            }
        }

        constexpr TransportParamError validate_transport_param(DefinedTransportParams& param) {
            if (param.ack_delay_exponent > 20) {
                return TransportParamError::over_20_ack_delay_exponent;
            }
            if (param.active_connection_id_limit < 2) {
                return TransportParamError::less_than_2_connection_id;
            }
            if (param.max_udp_payload_size < 1200) {
                return TransportParamError::less_than_1200_udp_payload;
            }
            if (param.max_ack_delay > (1 << 14)) {
                return TransportParamError::over_2pow14_max_ack_delay;
            }
            if (!param.preferred_address.connectionID.null() && param.preferred_address.connectionID.size() < 1) {
                return TransportParamError::zero_length_connid;
            }
            if (param.initial_max_streams_bidi >= size_t(1) << 60) {
                return TransportParamError::too_large_max_streams_bidi;
            }
            if (param.initial_max_streams_uni >= size_t(1) << 60) {
                return TransportParamError::too_large_max_streams_uni;
            }
            return TransportParamError::none;
        }

        // TransportParamStorage is boxing utility of DefinedTransportParams
        struct TransportParamStorage {
            storage boxed[4];

           private:
            bool do_boxing(storage& box, view::rvec& param) {
                if (!param.null() && box.data() != param.data()) {
                    box = make_storage(param);
                    if (box.null()) {
                        return false;
                    }
                    param = box;
                }
                return true;
            }

           public:
            bool boxing(DefinedTransportParams& params) {
                return do_boxing(boxed[0], params.original_dst_connection_id) &&
                       do_boxing(boxed[1], params.preferred_address.connectionID) &&
                       do_boxing(boxed[2], params.initial_src_connection_id) &&
                       do_boxing(boxed[3], params.retry_src_connection_id);
            }
        };

        constexpr stream::InitialLimits to_initial_limits(const DefinedTransportParams& params) {
            stream::InitialLimits limits;
            limits.conn_data_limit = params.initial_max_data;
            limits.bidi_stream_limit = params.initial_max_streams_bidi;
            limits.uni_stream_limit = params.initial_max_streams_uni;
            limits.bidi_stream_data_local_limit = params.initial_max_stream_data_bidi_local;
            limits.bidi_stream_data_remote_limit = params.initial_max_stream_data_bidi_remote;
            limits.uni_stream_data_limit = params.initial_max_stream_data_uni;
            return limits;
        }

    }  // namespace dnet::quic::trsparam
}  // namespace utils
