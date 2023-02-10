/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include "../../io/number.h"

namespace utils {
    namespace dnet::dns {
        enum class OpCode : byte {
            query,
            status,
            notify,
            update,
        };

        enum class ReturnCode : byte {
            no_error = 0,
            form_error = 1,
            serv_fail = 2,
            not_exist_domain = 3,
            not_implemented = 4,
            refused = 5,
            unexpected_existing_domain = 6,
            unexpected_resource_reset = 7,
            not_authoritative = 8,
            not_in_zone = 9,
        };

        struct Flag {
            std::uint16_t flag = 0;

            constexpr bool is_response() const noexcept {
                return (flag & 0x8000);
            }

            constexpr bool is_query() const noexcept {
                return !is_response();
            }

           private:
            constexpr void set_bit(std::uint16_t bit, bool q) noexcept {
                if (q) {
                    flag |= bit;
                }
                else {
                    flag &= ~bit;
                }
            }

           public:
            constexpr void set_response(bool qr) noexcept {
                set_bit(0x8000, qr);
            }

            constexpr OpCode op_code() const noexcept {
                return OpCode((flag & 0x7800) >> 11);
            }

            constexpr void set_op_code(OpCode code) noexcept {
                flag &= 0x87ff;
                flag |= (std::uint16_t(code) & 0xf) << 11;
            }

            constexpr bool authoritative() const noexcept {
                return (flag & 0x0400);
            }

            constexpr void set_authoritative(bool aa) noexcept {
                set_bit(0x0400, aa);
            }

            constexpr bool truncated() const noexcept {
                return (flag & 0x0200);
            }

            constexpr void set_truncated(bool tc) noexcept {
                set_bit(0x0200, tc);
            }

            constexpr bool recursion_desired() const noexcept {
                return (flag & 0x0100);
            }

            constexpr void set_recursion_desired(bool rd) noexcept {
                set_bit(0x0100, rd);
            }

            constexpr bool recursion_available() const noexcept {
                return (flag & 0x0080);
            }

            constexpr void set_recursion_available(bool ra) noexcept {
                set_bit(0x0080, ra);
            }

            constexpr void clear_z() noexcept {
                flag &= 0xFFBF;
            }

            constexpr bool dnssec() const noexcept {
                return (flag & 0x0040);
            }

            constexpr void set_dnssec(bool ad) noexcept {
                set_bit(0x0040, ad);
            }

            constexpr bool ban_dnssec() const noexcept {
                return (flag & 0x0020);
            }

            constexpr void set_ban_dnssec(bool cd) noexcept {
                set_bit(0x0020, cd);
            }

            constexpr ReturnCode return_code() const noexcept {
                return ReturnCode(flag & 0x000F);
            }

            constexpr void set_return_code(ReturnCode code) noexcept {
                flag &= 0xfff0;
                flag |= (std::uint16_t(code) & 0xf);
            }
        };

        struct Header {
            std::uint16_t id = 0;
            Flag flag;
            std::uint16_t query_count = 0;            // only parse
            std::uint16_t answer_count = 0;           // only parse
            std::uint16_t authority_count = 0;        // only parse
            std::uint16_t additional_info_count = 0;  // only parse

            constexpr bool parse(io::reader& r) noexcept {
                return io::read_num(r, id) &&
                       io::read_num(r, flag.flag) &&
                       io::read_num(r, query_count) &&
                       io::read_num(r, answer_count) &&
                       io::read_num(r, authority_count) &&
                       io::read_num(r, additional_info_count);
            }

            constexpr bool render(
                io::writer& w,
                std::uint64_t query, std::uint64_t answer, std::uint64_t authority, std::uint64_t additional) const noexcept {
                if (query > 0xffff ||
                    answer > 0xffff ||
                    authority > 0xffff ||
                    additional > 0xffff) {
                    return false;
                }
                return io::write_num(w, id) &&
                       io::write_num(w, flag.flag) &&
                       io::write_num(w, std::uint16_t(query)) &&
                       io::write_num(w, std::uint16_t(answer)) &&
                       io::write_num(w, std::uint16_t(authority)) &&
                       io::write_num(w, std::uint16_t(additional));
            }
        };

        // src:
        // https://en.wikipedia.org/w/index.php?title=List_of_DNS_record_types&oldid=679175573
        enum class RecordType : std::uint16_t {
            A = 1,
            AAAA = 28,
            AFSDB = 18,
            APL = 42,
            CAA = 257,
            CDNSKEY = 60,
            CDS = 59,
            CERT = 37,
            CNAME = 5,
            DHCID = 49,
            DLV = 32769,
            DNAME = 39,
            DNSKEY = 48,
            DS = 43,
            HIP = 55,
            IPSECKEY = 45,
            KEY = 25,
            KX = 36,
            LOC = 29,
            MX = 15,
            NAPTR = 35,
            NS = 2,
            NSEC = 47,
            NSEC3 = 50,
            NSEC3PARAM = 51,
            PTR = 12,
            RRSIG = 46,
            RP = 17,
            SIG = 24,
            SOA = 6,
            SRV = 33,
            SSHFP = 44,
            TA = 32768,
            TKEY = 249,
            TLSA = 52,
            TSIG = 250,
            TXT = 16,
            ANY = 255,
            AXFR = 252,
            IXFR = 451,
            OPT = 41,  // EDNS
            URI = 256,
            X25 = 19,
            WKS = 11,
            SVCB = 64,
            HTTPS = 65,
        };

        enum class DNSClass : std::uint16_t {
            IN = 1,
            CS = 2,  // deprecated
            CH = 3,
            HS = 4,
        };

        // label - label (without length field)
        // len - label len field
        // offset - compression offset
        // if offset is enable, len&0xC0 is true
        constexpr bool read_label(io::reader& r, byte& len, view::rvec& label, std::uint16_t* offset) {
            if (r.empty()) {
                return false;
            }
            len = r.top();
            if ((len & 0xC0) == 0xC0) {
                if (!offset) {
                    return false;
                }
                if (!io::read_num(r, *offset)) {
                    return false;
                }
                *offset &= 0x3FFF;
                return true;
            }
            else if (len > 63) {
                return false;
            }
            r.offset(1);
            return r.read(label, len);
        }

        // this not resove compressed labels
        constexpr bool read_labels(io::reader& r, view::rvec& name, bool allow_compress) {
            io::reader p{r.remain()};
            while (true) {
                byte len = 0;
                std::uint16_t offset = 0;
                if (!read_label(p, len, name, allow_compress ? &offset : nullptr)) {
                    return false;
                }
                if (len == 0 || len & 0xC0) {
                    break;
                }
            }
            if (p.offset() > 0xff) {
                return false;
            }
            return r.read(name, p.offset());
        }

        // not allow compressed
        constexpr bool render_uri_labels(io::writer& w, view::rvec labels) {
            if (labels.size() > 253) {
                return false;
            }
            io::reader r{labels};
            auto read_label = [&] {
                io::reader p{r.remain()};
                view::rvec data;
                while (!p.empty()) {
                    if (p.top() == '.') {
                        r.read(data, p.offset());
                        r.offset(1);
                        break;
                    }
                    p.offset(1);
                }
                if (data.null()) {
                    r.read(data, p.offset());
                }
                return data;
            };
            while (true) {
                auto label = read_label();
                if (label.size() == 0 && !r.empty()) {
                    return false;
                }
                if (label.size() > 63) {
                    return false;
                }
                if (!w.write(byte(label.size()), 1) ||
                    !w.write(label)) {
                    return false;
                }
                if (label.size() == 0) {
                    break;
                }
            }
            return true;
        }

        struct Name {
            view::rvec name;
            bool is_raw_format = false;
            constexpr bool parse(io::reader& r) noexcept {
                if (!read_labels(r, name, true)) {
                    return false;
                }
                is_raw_format = true;
                return true;
            }

            constexpr size_t len() const noexcept {
                return is_raw_format ? name.size() : name.size() + 2;
            }

            constexpr bool render(io::writer& w) const noexcept {
                if (is_raw_format) {
                    // check
                    io::reader r{name};
                    view::rvec tmp;
                    if (!read_labels(r, tmp, true) || !r.empty()) {
                        return false;
                    }
                    return w.write(name);
                }
                else {
                    return render_uri_labels(w, name);
                }
            }
        };

        struct Query {
            Name name;
            RecordType type{};
            DNSClass dns_class = DNSClass::IN;

            constexpr bool parse(io::reader& r) noexcept {
                return name.parse(r) &&
                       io::read_num(r, type) &&
                       io::read_num(r, dns_class);
            }

            constexpr bool render(io::writer& w) const noexcept {
                return name.render(w) &&
                       io::write_num(w, type) &&
                       io::write_num(w, dns_class);
            }
        };

        struct Record {
            Name name;
            RecordType type{};
            DNSClass dns_class{};
            std::uint32_t ttl = 0;
            std::uint16_t data_len = 0;  // only parse
            view::rvec data;

            constexpr bool parse(io::reader& r) noexcept {
                return name.parse(r) &&
                       io::read_num(r, type) &&
                       io::read_num(r, dns_class) &&
                       io::read_num(r, ttl) &&
                       io::read_num(r, data_len) &&
                       r.read(data, data_len);
            }

            constexpr size_t len() const noexcept {
                return name.len() + 2 + 2 + 4 + 2 + data.size();
            }

            constexpr bool render(io::writer& w) const noexcept {
                if (data.size() > 0xffff) {
                    return false;
                }
                return name.render(w) &&
                       io::write_num(w, type) &&
                       io::write_num(w, dns_class) &&
                       io::write_num(w, ttl) &&
                       io::write_num(w, std::uint16_t(data.size())) &&
                       w.write(data);
            }
        };

        template <template <class...> class Vec>
        struct Message {
            Header header;
            Vec<Query> query;
            Vec<Record> answer;
            Vec<Record> authority;
            Vec<Record> additional;

            constexpr bool parse(io::reader& r) noexcept {
                if (!header.parse(r)) {
                    return false;
                }
                query.clear();
                for (size_t i = 0; i < header.query_count; i++) {
                    Query q;
                    if (!q.parse(r)) {
                        return false;
                    }
                    query.push_back(std::move(q));
                }
                auto each_record = [&](std::uint16_t count, Vec<Record>& recs) {
                    recs.clear();
                    for (size_t i = 0; i < count; i++) {
                        Record rec;
                        if (!rec.parse(r)) {
                            return false;
                        }
                        recs.push_back(std::move(rec));
                    }
                    return true;
                };
                return each_record(header.answer_count, answer) &&
                       each_record(header.authority_count, authority) &&
                       each_record(header.additional_info_count, additional);
            }

            constexpr bool render(io::writer& w) const noexcept {
                if (!header.render(w, query.size(), answer.size(), authority.size(), additional.size())) {
                    return false;
                }
                for (size_t i = 0; i < query.size(); i++) {
                    Query q = query[i];
                    if (!q.render(w)) {
                        return false;
                    }
                }
                auto each_record = [&](const Vec<Record>& recs) {
                    for (size_t i = 0; i < recs.size(); i++) {
                        Record rec = recs[i];
                        if (!rec.render(w)) {
                            return false;
                        }
                    }
                    return true;
                };
                return each_record(answer) &&
                       each_record(authority) &&
                       each_record(additional);
            }
        };

        template <class String>
        constexpr bool resolve_name(String& resolved, Name name, view::rvec full_dns_message, bool as_uri_host) {
            resolved.clear();
            if (!name.is_raw_format) {
                return false;  // already resolved?
            }
            std::uint16_t total = 0;
            std::uint16_t offset = 0;
            io::reader r{name.name};
            while (true) {
                byte len = 0;
                view::rvec label;
                if (!read_label(r, len, label, &offset)) {
                    return false;
                }
                // offset field
                if (len & 0xC0) {
                    auto next = full_dns_message.substr(offset);
                    if (next.empty()) {
                        return false;  // anti pattern 1: out of DNS message reference
                    }
                    if (next.front() & 0xC0) {
                        return false;  // anti pattern 2: nested pointer
                    }
                    auto range_begin = r.read().begin();
                    auto range_end = r.read().end();
                    if (range_begin <= next.data() && next.data() < range_end) {
                        return false;  // anti pattern 3: loop or self reference
                    }
                    r.reset(next);
                    continue;
                }
                total += 1 + len;
                if (total > 0xff) {
                    return false;  // anti pattern 4: over size
                }
                if (as_uri_host) {
                    if (len != 0 && total != 1 + len) {
                        resolved.push_back('.');
                    }
                }
                else {
                    resolved.push_back(len);
                }
                if (len == 0) {
                    break;
                }
                for (auto c : label) {
                    resolved.push_back(c);
                }
            }
            return true;
        }

    }  // namespace dnet::dns
}  // namespace utils
