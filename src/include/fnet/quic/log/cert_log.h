/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../tls/x509.h"

namespace futils {
    namespace fnet::quic::log {
        void format_certificate(auto& out, view::rvec data, bool omit_data, bool data_as_hex) {
            x509::parser::Certificate cert;
            asn1::TLV tlv;
            binary::reader r{data};
            const auto datafield = fmt_datafield(out, omit_data, data_as_hex);
            const auto hexfield = fmt_hexfield(out);
            const auto custom = fmt_key_value(out);
            const auto group = fmt_group(out);
            const auto typefield = [&](auto type) {
                if (auto str = to_string(type)) {
                    strutil::appends(out, str);
                }
                else {
                    strutil::append(out, "unknown(0x");
                    number::to_string(out, std::uint16_t(type), 16);
                    strutil::append(out, ")");
                }
            };
            const auto oid_field = [&](asn1::OID oid) {
                std::uint64_t value[4];
                size_t i = 0;
                oid.iterate([&](std::uint64_t c, bool last) {
                    if (i < 4) {
                        value[i] = c;
                    }
                    i++;
                    number::to_string(out, c);
                    if (!last) {
                        strutil::append(out, ".");
                    }
                });
                if (i == 4) {
                    if (value[0] == 2 && value[1] == 5 && value[2] == 4) {
                        switch (value[3]) {
                            case 3:
                                strutil::append(out, "(CN)");
                                break;
                            case 6:
                                strutil::append(out, "(C)");
                                break;
                            case 7:
                                strutil::append(out, "(L)");
                                break;
                            case 8:
                                strutil::append(out, "(ST)");
                                break;
                            case 10:
                                strutil::append(out, "(O)");
                                break;
                            case 11:
                                strutil::append(out, "(OU)");
                                break;
                            default:
                                break;
                        }
                    }
                }
            };
            const auto format_names = [&](const char* name, x509::parser::RDNSequence seq, bool last_comma) {
                group(name, false, last_comma, [&] {
                    seq.iterate([&](x509::parser::RelativeDistinguishedName rdn, bool last) {
                        rdn.iterate([&](x509::parser::AttributeTypeAndValue atv, bool) {
                            custom([&] { oid_field(atv.type); }, [&] { datafield(nullptr, atv.value.data, false); }, !last);
                        });
                    });
                });
            };
            if (!tlv.parse(r) || !cert.parse(tlv)) {
                datafield(nullptr, data);
                return;
            }
            group(nullptr, false, false, [&] {
                custom([&] { strutil::append(out, "version"); }, [&] { typefield(cert.tbsCertificate.version); });
                hexfield("serial_number", cert.tbsCertificate.certificate_serial_number.data());
                format_names("issuer", cert.tbsCertificate.issuer, true);
                format_names("subject", cert.tbsCertificate.subject, false);
            });
        }
    }  // namespace fnet::quic::log
}  // namespace futils
