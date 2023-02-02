/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto_log - QUIC tls log
#pragma once
#include "../../tls_head.h"
#include "format.h"

namespace utils {
    namespace dnet::quic::log {
        constexpr void format_crypto(auto& out, view::rvec src, bool omit_data, bool data_as_hex) {
#define SWITCH()           \
    if constexpr (false) { \
    }
#define CASE(T) else if constexpr (std::is_same_v<const T&, decltype(hs)>)
            io::reader r{src};
            auto typefield = [](auto type) {
                if (auto type = to_string(type)) {
                    helper::appends(out, type);
                }
                else {
                    helper::append(out, "unknown(0x");
                    number::to_string(out, std::uint16_t(type), 16);
                    helper::append(out, ")");
                }
            };
            const auto numfield = fmt_numfield(out);
            const auto hexfield = fmt_hexfield(out);
            const auto datafield = fmt_datafield(out, omit_data, data_as_hex);
            auto format_extension = [&](const tls::handshake::ext::Extension& ext, const auto& hs, bool err) {
                typefield(ext.extension_type);
                helper::append(" {");
                SWITCH()
                CASE(tls::handshake::ext::Extension) {
                    datafield("opaque", ext.data());
                }
                CASE(tls::handshake::ext::QUICTransportParamExtension) {
                    const tls::handshake::ext::QUICTransportParamExtension& q = hs;
                    helper::append(" transport_parameters: [");
                    q.params.iterate([&](quic::trsparam::TransportParameter param) {
                        helper::append(out, " { id: ");
                        typefield((quic::trsparam::DefinedID)param.id);
                        helper::append(out, ",");
                        numfield("length", param.length);
                        hexfield("data", param.data);
                        helper::append(out, "},");
                    });
                    helper::append("],");
                }
                CASE(tls::handshake::ext::ProtocolNameList) {
                    const tls::handshake::ext::ProtocolNameList& q = hs;
                    helper::append(" protocol_name_list: [");
                    helper::append("],");
                }
                helper::append("} ");
            };
            while (tls::handshake::parse_one(r, [&](const auto& hs, bool err) {
                if (err) {
                    helper::append(out, "error at parsing...\n");
                    return false;
                }
                typefield(hs.msg_type);
                helper::append(out, " {");
                SWITCH()
                CASE(tls::handshake::ClientHello) {
                    const tls::handshake::ClientHello& ch = hs;
                }
                helper::append(out, "} \n");
            })) {
            }
#undef SWITCH
#undef CASE
        }
    }  // namespace dnet::quic::log
}  // namespace utils
