/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto_log - QUIC tls log
#pragma once
#include "../../tls/parse.h"
#include "format.h"
#include"cert_log.h"

namespace futils {
    namespace fnet::quic::log {
        constexpr void format_crypto(auto& out, view::rvec src, bool omit_data, bool data_as_hex) {
#define SWITCH()           \
    if constexpr (false) { \
    }
#define CASE(T) else if constexpr (std::is_same_v<const T&, decltype(hs)>)
            binary::reader r{src};
            auto typefield = [&](auto type) {
                if (auto str = to_string(type)) {
                    strutil::appends(out, str);
                }
                else {
                    strutil::append(out, "unknown(0x");
                    number::to_string(out, std::uint16_t(type), 16);
                    strutil::append(out, ")");
                }
            };
            const auto numfield = fmt_numfield(out);
            const auto hexfield = fmt_hexfield(out);
            const auto datafield = fmt_datafield(out, omit_data, data_as_hex);
            const auto group = fmt_group(out);
            const auto custom_key_value = fmt_key_value(out);
            auto format_extension = [&](const tls::handshake::ext::Extension& ext,tls::handshake::HandshakeType ty,bool last_comma) {
                tls::handshake::ext::parse(ext,ty,[&](const auto& hs,bool err){
                    custom_key_value([&]{typefield(ext.extension_type);},[&]{
                        group(nullptr, false,last_comma, [&] {
                            SWITCH()
                            CASE(tls::handshake::ext::Extension) {
                                datafield("opaque", ext.data());
                            }
                            CASE(tls::handshake::ext::QUICTransportParamExtension) {
                                const tls::handshake::ext::QUICTransportParamExtension& q = hs;
                                group("transport_parameters", true, false, [&] {
                                    q.params.iterate([&](quic::trsparam::TransportParameter param,bool last) {
                                        group(nullptr, false, !last, [&] {
                                            custom_key_value([&]{strutil::append(out,"id");},[&]{ typefield((quic::trsparam::DefinedID)param.id);});
                                            numfield("length", param.length);
                                            hexfield("data", param.data.bytes(),false);
                                        });
                                        return true;
                                    });
                                });
                            }
                            CASE(tls::handshake::ext::ProtocolNameList) {
                                const tls::handshake::ext::ProtocolNameList& q = hs;
                                group("protocol_name_list", true, false, [&] {
                                    q.protocol_name_list.iterate([&](tls::handshake::ext::ProtocolName name,bool last){
                                        datafield(nullptr,name.data(),!last);
                                        return true;
                                    });
                                });
                            }
                            return true;
                        });
                    },false);
                    return true;
                });
            };
            auto format_extensions=[&](tls::handshake::HandshakeType type,auto& exts){
                group("extensions", true, false, [&] {
                    exts.iterate([&](const tls::handshake::ext::Extension& ext,bool last) {
                        format_extension(ext,type,!last);
                        return true;
                    });
                });
            };
            while (tls::handshake::parse_one(r, [&](const auto& hs, bool err) {
                if (err) {
                    strutil::append(out, "error at parsing...\n");
                    return true;
                }
                typefield(hs.msg_type);
                group(nullptr, false, false, [&] {
                    numfield("length", hs.length);
                    SWITCH()
                    CASE(tls::handshake::ClientHello) {
                        const tls::handshake::ClientHello& ch = hs;
                        hexfield("random", ch.random);
                        group("cipher_suites", true, true, [&] {
                            const auto size = ch.cipher_suites.size();
                            for (size_t i = 0; i < size; i++) {
                                typefield(ch.cipher_suites[i]);
                                strutil::append(out, ",");
                            }
                        });
                      format_extensions(ch.msg_type,ch.extensions);
                    }
                    CASE(tls::handshake::ServerHello) {
                        const tls::handshake::ServerHello& sh = hs;
                        hexfield("random", sh.random);
                        custom_key_value([&]{strutil::append(out,"cipher_suite");},[&]{ typefield(sh.cipher_suite);});
                        format_extensions(sh.msg_type,sh.extensions);
                    }
                    CASE(tls::handshake::EncryptedExtensions){
                        const tls::handshake::EncryptedExtensions& ee=hs;
                        format_extensions(ee.msg_type,ee.extensions);
                    }
                    CASE(tls::handshake::Certificate){
                        const tls::handshake::Certificate& cert=hs;
                        hexfield("certificate_request_context",cert.certificate_request_context.data());
                        group("certificate_list",true,false,[&]{
                            cert.certificate_list.iterate([&](const tls::handshake::CertificateEntry& ent,bool last ){
                                group(nullptr,false,!last,[&]{
                                    custom_key_value([&]{strutil::append(out,"cert_data");},[&]{
                                        format_certificate(out,ent.cert_data.data(),omit_data,data_as_hex);
                                    });
                                    format_extensions(cert.msg_type,ent.extensions);
                                });
                                return true;
                            });                  
                        });
                    }
                    CASE(tls::handshake::Unknown){
                        const tls::handshake::Unknown& ch=hs;
                        datafield("raw_data",ch.raw_data,false);
                    }
                });
                strutil::append(out, "\n");
                return true;
            })) {
            }
#undef SWITCH
#undef CASE
        }
    }  // namespace fnet::quic::log
}  // namespace futils
