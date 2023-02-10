/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// X509
// der encoding subset for x509 certificate parser
#pragma once
#include "../../io/number.h"

namespace utils {
    namespace dnet::x509 {
        enum class Tag : byte {
            RESERVED = 0x0,
            BOOLEAN = 0x01,
            INTEGER = 0x02,
            BIT_STRING = 0x03,
            OCTET_STRING = 0x04,
            NULL_ = 0x05,
            OBJECT_IDENTIFIER = 0x06,
            UTF8String = 0x0C,
            SEQUENCE = 0x30,
            SET = 0x31,
            PrintableString = 0x13,
            IA5String = 0x16,
            UTCTime = 0x17,
            GeneralizedTime = 0x18,
        };

        enum class TagClass : byte {
            universal = 0b00,
            application = 0b01,
            context_specific = 0b10,
            private_ = 0b11,
        };

        constexpr TagClass tag_class(Tag tag) noexcept {
            return TagClass((byte(tag) >> 6) & 0x3);
        }

        constexpr bool is_structured(Tag tag) noexcept {
            return byte(tag) & 0x20;
        }

        constexpr Tag ctx_spec(byte n, bool structured = false) noexcept {
            return Tag((byte(TagClass::context_specific) << 6) | (structured ? 0x20 : 0x00) | (n & 0x1f));
        }

        struct Len {
           private:
            std::uint64_t len = 0;

           public:
            constexpr Len() = default;

            constexpr operator std::uint64_t() const {
                return len;
            }

            constexpr bool parse(io::reader& r) noexcept {
                if (r.empty()) {
                    return false;
                }
                if (r.top() < 0x80) {
                    len = r.top();
                    r.offset(1);
                    return true;
                }
                const auto len_len = (r.top() & 0x7F);
                if (len_len > 8) {
                    // this means, r has more data than it can count
                    // but such things would not occur
                    // use other system !
                    return false;
                }
                r.offset(1);
                auto [data, ok] = r.read(len_len);
                if (!ok) {
                    return false;
                }
                len = 0;
                for (size_t i = 0; i < len_len; i++) {
                    len |= (data[i] << ((len_len - i - 1) * 8));
                }
                return true;
            }
        };

        struct TLV {
            Tag tag{};
            Len len;
            view::rvec data;

            constexpr bool parse(io::reader& r) {
                return io::read_num(r, tag) &&
                       len.parse(r) &&
                       r.read(data, len);
            }

            constexpr bool iterate(size_t least, auto&& cb) const {
                io::reader r{data};
                size_t count = 0;
                while (!r.empty()) {
                    TLV tlv;
                    if (!tlv.parse(r)) {
                        return false;
                    }
                    if (!cb(tlv, count, r.empty())) {
                        return false;
                    }
                    count++;
                }
                return count < least ? false : true;
            }

            constexpr std::pair<TLV, bool> explicit_unstruct() const {
                if (!is_structured(tag)) {
                    return {{}, false};
                }
                TLV tlv;
                io::reader r{data};
                if (!tlv.parse(r) || !r.empty()) {
                    return {{}, false};  // not single strucutured
                }
                return {tlv, true};
            }
        };

        struct Integer {
           private:
            view::rvec data_;

           public:
            constexpr Integer() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::INTEGER) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr bool sign() const {
                return data_.size() ? data_.front() & 0x80 : false;
            }

            constexpr std::pair<std::uint64_t, bool> as_uint64() const {
                if (data_.empty() || data_.size() > 9 || sign()) {
                    return {0, false};
                }
                if (data_.size() == 9) {
                    if (data_.front() != 0) {
                        return {0, false};
                    }
                }
                std::uint64_t result = 0;
                size_t i = 0;
                if (data_.size() == 9) {
                    i = 1;
                }
                for (; i < data_.size(); i++) {
                    result |= std::uint64_t(data_[i] & (i == 0 ? 0x7F : 0xFF)) << ((data_.size() - i - 1) * 8);
                }
                return {result, true};
            }

            constexpr std::pair<std::int64_t, bool> as_int64() const {
                if (data_.empty() || data_.size() > 8 || sign()) {
                    return {0, false};
                }
                std::uint64_t result = 0;
                for (size_t i = 0; i < data_.size(); i++) {
                    result |= std::uint64_t(data_[i] & (i == 0 ? 0x7F : 0xFF)) << ((data_.size() - i - 1) * 8);
                }
                std::int64_t out = result;
                if (sign()) {
                    out = -out;
                }
                return {out, true};
            }

            constexpr view::rvec data() {
                return data_;
            }
        };

        struct Bool {
            bool value = false;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::BOOLEAN) {
                if (tlv.tag != expect) {
                    return false;
                }
                if (tlv.data.size() != 1) {
                    return false;
                }
                value = tlv.data[0] != 0;
                return true;
            }
        };

        struct OID {
           private:
            view::rvec data_;

           public:
            constexpr OID() = default;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::OBJECT_IDENTIFIER) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr bool iterate(auto&& cb) {
                for (size_t i = 0; i < data_.size(); i++) {
                    if (i == 0) {
                        cb(data_[0] / 40, false);
                        cb(data_[0] % 40, i + 1 == data_.size());
                    }
                    else {
                        std::uint64_t val = data_[i] & 0x7F;
                        for (; i <= data_.size(); i++) {
                            if (i == data_.size()) {
                                return false;
                            }
                            if (data_[i] & 0x80) {
                                val <<= 7;  // val *= 128
                                val |= data_[i] & 0x7f;
                                continue;
                            }
                            break;
                        }
                        cb(val, i + 1 == data_.size());
                    }
                }
                return true;
            }

            constexpr bool to_array(auto&& to) {
                return iterate([&](auto num, bool last) {
                    to.push_back(num);
                });
            }

            constexpr view::rvec data() {
                return data_;
            }
        };

        struct BitString {
           private:
            view::rvec data_;

           public:
            constexpr BitString() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::BIT_STRING) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr byte unused_bits() const {
                return data_.empty() ? 0 : data_[0];
            }

            constexpr bool iterate(auto&& cb) const {
                if (data_.empty()) {
                    return false;
                }
                auto unused = unused_bits();
                if (unused > 7) {
                    return false;
                }
                if (data_.size() > 3) {
                    for (size_t i = 0; i < data_.size() - 2; i++) {
                        cb(bool(data_[i] & 0x80));
                        cb(bool(data_[i] & 0x40));
                        cb(bool(data_[i] & 0x20));
                        cb(bool(data_[i] & 0x10));
                        cb(bool(data_[i] & 0x08));
                        cb(bool(data_[i] & 0x04));
                        cb(bool(data_[i] & 0x02));
                        cb(bool(data_[i] & 0x01));
                    }
                }
                const auto i = data_.size() - 1;
                cb(bool(data_[i] & 0x80));
                if (unused == 7) return true;
                cb(bool(data_[i] & 0x40));
                if (unused == 6) return true;
                cb(bool(data_[i] & 0x20));
                if (unused == 5) return true;
                cb(bool(data_[i] & 0x10));
                if (unused == 4) return true;
                cb(bool(data_[i] & 0x08));
                if (unused == 3) return true;
                cb(bool(data_[i] & 0x04));
                if (unused == 2) return true;
                cb(bool(data_[i] & 0x02));
                if (unused == 1) return true;
                cb(bool(data_[i] & 0x01));
            }

            constexpr view::rvec data() {
                return data_;
            }
        };

        struct OctectString {
           private:
            view::rvec data_;

           public:
            constexpr OctectString() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::OCTET_STRING) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr view::rvec data() {
                return data_;
            }
        };

        struct UTCTime {
           private:
            view::rvec data_;

           public:
            constexpr UTCTime() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::UTCTime) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr view::rvec data() const {
                return data_;
            }
        };

        struct GeneralizedTime {
           private:
            view::rvec data_;

           public:
            constexpr GeneralizedTime() = default;

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::GeneralizedTime) {
                if (tlv.tag != expect) {
                    return false;
                }
                data_ = tlv.data;
                return true;
            }

            constexpr view::rvec data() const {
                return data_;
            }
        };

        struct Time {
           private:
            TLV tlv_;

           public:
            constexpr Time() = default;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::UTCTime && tlv.tag != Tag::GeneralizedTime) {
                    return false;
                }
                tlv_ = tlv;
                return true;
            }

            std::pair<UTCTime, bool> as_UTCTime() const {
                UTCTime t;
                if (!t.parse(tlv_)) {
                    return {{}, false};
                }
                return {t, true};
            }

            std::pair<GeneralizedTime, bool> as_GeneralizedTime() const {
                GeneralizedTime t;
                if (!t.parse(tlv_)) {
                    return {{}, false};
                }
                return {t, true};
            }
        };

        struct Validity {
            Time not_before;
            Time not_after;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(2, [&](const TLV& tlv, size_t i, bool last) {
                    if (i == 0) {
                        return not_before.parse(tlv);
                    }
                    else if (i == 1) {
                        return not_after.parse(tlv) && last;
                    }
                    return false;
                });
            }
        };

        struct AlgorithmIdentifier {
            OID algorithm;
            TLV parameters;  // optional

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(1, [&](const TLV& tlv, size_t i, bool last) {
                    if (i == 0) {
                        if (!algorithm.parse(tlv)) {
                            return false;
                        }
                        return true;
                    }
                    if (i == 1) {
                        parameters = tlv;
                        return last;
                    }
                    return false;
                });
            }
        };

        struct AttributeTypeAndValue {
            OID type;
            TLV value;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(2, [&](const TLV& tlv, size_t i, bool last) {
                    switch (i) {
                        case 0:
                            return type.parse(tlv) && !last;
                        case 1:
                            value = tlv;
                            return last;
                        default:
                            return false;
                    }
                });
            }
        };

        struct RelativeDistinguishedName {
           private:
            TLV tlv_;

           public:
            constexpr bool iterate(auto&& cb) const {
                return tlv_.iterate(1, [&](const TLV& tlv, size_t i, bool last) {
                    AttributeTypeAndValue value;
                    if (!value.parse(tlv)) {
                        return false;
                    }
                    cb(value, last);
                    return true;
                });
            }

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::SET) {
                    return false;
                }
                tlv_ = tlv;
                return iterate([](auto...) {});
            }
        };

        struct RDNSequence {
           private:
            TLV tlv_;

           public:
            constexpr bool iterate(auto&& cb) {
                return tlv_.iterate(0, [&](const TLV& tlv, size_t i, bool last) {
                    RelativeDistinguishedName name;
                    if (!name.parse(tlv)) {
                        return false;
                    }
                    cb(name, last);
                    return true;
                });
            }

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::SEQUENCE) {
                    return false;
                }
                tlv_ = tlv;
                return iterate([](auto...) {});
            }
        };

        struct SubjectPublicKeyInfo {
            AlgorithmIdentifier algorithm;
            BitString subjectPublicKey;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(2, [&](const TLV& tlv, size_t i, bool last) {
                    switch (i) {
                        case 0:
                            return algorithm.parse(tlv);
                        case 1:
                            return subjectPublicKey.parse(tlv) && last;
                        default:
                            return false;
                    }
                });
            }
        };

        struct Extension {
            OID extnID;
            Bool critical;
            OctectString extnValue;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(2, [&](const TLV& tlv, size_t i, bool last) {
                    switch (i) {
                        case 0:
                            return extnID.parse(tlv);
                        case 1: {
                            if (tlv.tag == Tag::BOOLEAN) {
                                return critical.parse(tlv) && !last;
                            }
                        }
                            [[fallthrough]];
                        case 2:
                            return extnValue.parse(tlv) && last;
                        default:
                            return false;
                    }
                });
            }
        };

        struct Extensions {
           private:
            TLV tlv_;

           public:
            constexpr bool iterate(auto&& cb) const {
                return tlv_.iterate(0, [&](const TLV& tlv, size_t i, bool last) {
                    Extension ext;
                    if (!ext.parse(tlv)) {
                        return false;
                    }
                    cb(ext);
                    return true;
                });
            }

            constexpr bool parse(const TLV& tlv, Tag expect = Tag::SEQUENCE) {
                if (tlv.tag != expect) {
                    return false;
                }
                return iterate([](auto...) {});
            }
        };

        enum class Version {
            v1 = 0x0,
            v2 = 0x1,
            v3 = 0x2,
        };

        constexpr const char* to_string(Version ver) {
            switch (ver) {
                case Version::v1:
                    return "v1";
                case Version::v2:
                    return "v2";
                case Version::v3:
                    return "v3";
                default:
                    return nullptr;
            }
        }

        struct TBSCertificate {
            Version version = Version::v1;
            Integer certificate_serial_number;
            AlgorithmIdentifier signature;
            RDNSequence issuer;
            Validity validity;
            RDNSequence subject;
            SubjectPublicKeyInfo subjectPublickeyInfo;
            BitString issuerUniqueID;
            BitString subjectUniqueID;
            Extensions extensions;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::SEQUENCE) {
                    return false;
                }
                size_t next = 0;
                return tlv.iterate(1, [&](const TLV& tlv, size_t i, bool last) {
                    switch (next) {
                        case 0:
                            next = 1;
                            if (tlv.tag == ctx_spec(0, true)) {
                                auto [t, ok1] = tlv.explicit_unstruct();
                                if (!ok1) {
                                    return false;
                                }
                                Integer tmp;
                                if (!tmp.parse(t)) {
                                    return false;
                                }
                                auto [p, ok2] = tmp.as_uint64();
                                if (!ok2) {
                                    return false;
                                }
                                version = Version(p);
                                return true;
                            }
                            [[fallthrough]];
                        case 1:
                            next = 2;
                            return certificate_serial_number.parse(tlv) && !last;
                        case 2:
                            next = 3;
                            return signature.parse(tlv) && !last;
                        case 3:
                            next = 4;
                            return issuer.parse(tlv) && !last;
                        case 4:
                            next = 5;
                            return validity.parse(tlv) && !last;
                        case 5:
                            next = 6;
                            return subject.parse(tlv) && !last;
                        case 6:
                            next = 7;
                            return subjectPublickeyInfo.parse(tlv);
                        case 7:
                            next = 8;
                            if (tlv.tag == ctx_spec(1)) {
                                return issuerUniqueID.parse(tlv, ctx_spec(1));
                            }
                            [[fallthrough]];
                        case 8:
                            next = 9;
                            if (tlv.tag == ctx_spec(2)) {
                                return subjectUniqueID.parse(tlv, ctx_spec(2));
                            }
                            [[fallthrough]];
                        case 9:
                            next = 10;
                            if (tlv.tag == ctx_spec(3, true)) {
                                return extensions.parse(tlv, ctx_spec(3, true)) && last;
                            }
                            [[fallthrough]];
                        default:
                            return false;
                    }
                });
            }
        };

        struct Certificate {
            TBSCertificate tbsCertificate;
            AlgorithmIdentifier signatureAlgorithm;
            BitString signature;

            constexpr bool parse(const TLV& tlv) {
                if (tlv.tag != Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(3, [&](const TLV& tlv, size_t i, bool last) {
                    switch (i) {
                        case 0:
                            return tbsCertificate.parse(tlv) && !last;
                        case 1:
                            return signatureAlgorithm.parse(tlv) && !last;
                        case 2:
                            return signature.parse(tlv) && last;
                        default:
                            return false;
                    }
                });
            }
        };
    }  // namespace dnet::x509
}  // namespace utils
