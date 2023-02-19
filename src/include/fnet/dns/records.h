/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "dns.h"
#include "../../io/expandable_writer.h"

namespace utils {
    namespace fnet::dns {
        struct ARecord {
            byte address[4];
            constexpr bool parse(const Record& r) noexcept {
                if (r.type != RecordType::A) {
                    return false;
                }
                io::reader p{r.data};
                return p.read(address) && p.empty();
            }

            constexpr bool render(io::writer& w) const noexcept {
                return w.write(address);
            }
        };

        struct AAAARecord {
            byte address[16];
            constexpr bool parse(const Record& r) noexcept {
                if (r.type != RecordType::AAAA) {
                    return false;
                }
                io::reader p{r.data};
                return p.read(address) && p.empty();
            }

            constexpr bool render(io::writer& w) const noexcept {
                return w.write(address);
            }
        };

        struct EDNSRecord {
            std::uint16_t mtu = 0;
            byte ext_rcode = 0;
            byte version = 0;
            std::uint16_t flags = 0;
            constexpr bool parse(const Record& r) noexcept {
                if (r.type != RecordType::OPT) {
                    return false;
                }
                mtu = std::uint16_t(r.dns_class);
                ext_rcode = byte((r.ttl >> 24) & 0xff);
                version = byte((r.ttl >> 16) & 0xff);
                flags = r.ttl & 0xffff;
                return true;
            }

            constexpr void pack(Record& r) {
                r.name = {};
                r.type = RecordType::OPT;
                r.dns_class = DNSClass(mtu);
                r.ttl = 0;
                r.ttl |= std::uint32_t(ext_rcode) << 24;
                r.ttl |= std::uint32_t(version) << 16;
                r.ttl |= flags;
                r.data = {};
            }
        };

    }  // namespace fnet::dns
}  // namespace utils
