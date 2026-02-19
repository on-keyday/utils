/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// X509
// der encoding subset for x509 certificate parser
#pragma once
#include "../../binary/number.h"
#include "asn1.h"

namespace futils {
    namespace fnet::x509::parser {

        struct Validity {
            asn1::Time not_before;
            asn1::Time not_after;

            constexpr bool parse(const asn1::TLV& tlv) {
                if (tlv.tag != asn1::Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(2, [&](const asn1::TLV& tlv, size_t i, bool last) {
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
            asn1::OID algorithm;
            asn1::TLV parameters;  // optional

            constexpr bool parse(const asn1::TLV& tlv) {
                if (tlv.tag != asn1::Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(1, [&](const asn1::TLV& tlv, size_t i, bool last) {
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
            asn1::OID type;
            asn1::TLV value;

            constexpr bool parse(const asn1::TLV& tlv) {
                if (tlv.tag != asn1::Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(2, [&](const asn1::TLV& tlv, size_t i, bool last) {
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
            asn1::TLV tlv_;

           public:
            constexpr bool iterate(auto&& cb) const {
                return tlv_.iterate(1, [&](const asn1::TLV& tlv, size_t i, bool last) {
                    AttributeTypeAndValue value;
                    if (!value.parse(tlv)) {
                        return false;
                    }
                    cb(value, last);
                    return true;
                });
            }

            constexpr bool parse(const asn1::TLV& tlv) {
                if (tlv.tag != asn1::Tag::SET) {
                    return false;
                }
                tlv_ = tlv;
                return iterate([](auto...) {});
            }
        };

        struct RDNSequence {
           private:
            asn1::TLV tlv_;

           public:
            constexpr bool iterate(auto&& cb) {
                return tlv_.iterate(0, [&](const asn1::TLV& tlv, size_t i, bool last) {
                    RelativeDistinguishedName name;
                    if (!name.parse(tlv)) {
                        return false;
                    }
                    cb(name, last);
                    return true;
                });
            }

            constexpr bool parse(const asn1::TLV& tlv) {
                if (tlv.tag != asn1::Tag::SEQUENCE) {
                    return false;
                }
                tlv_ = tlv;
                return iterate([](auto...) {});
            }
        };

        struct SubjectPublicKeyInfo {
            AlgorithmIdentifier algorithm;
            asn1::BitString subjectPublicKey;

            constexpr bool parse(const asn1::TLV& tlv) {
                if (tlv.tag != asn1::Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(2, [&](const asn1::TLV& tlv, size_t i, bool last) {
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
            asn1::OID extnID;
            asn1::Bool critical;
            asn1::OctectString extnValue;

            constexpr bool parse(const asn1::TLV& tlv) {
                if (tlv.tag != asn1::Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(2, [&](const asn1::TLV& tlv, size_t i, bool last) {
                    switch (i) {
                        case 0:
                            return extnID.parse(tlv);
                        case 1: {
                            if (tlv.tag == asn1::Tag::BOOLEAN) {
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
            asn1::TLV tlv_;

           public:
            constexpr bool iterate(auto&& cb) const {
                return tlv_.iterate(0, [&](const asn1::TLV& tlv, size_t i, bool last) {
                    Extension ext;
                    if (!ext.parse(tlv)) {
                        return false;
                    }
                    cb(ext);
                    return true;
                });
            }

            constexpr bool parse(const asn1::TLV& tlv, asn1::Tag expect = asn1::Tag::SEQUENCE) {
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
            asn1::Integer certificate_serial_number;
            AlgorithmIdentifier signature;
            RDNSequence issuer;
            Validity validity;
            RDNSequence subject;
            SubjectPublicKeyInfo subjectPublickeyInfo;
            asn1::BitString issuerUniqueID;
            asn1::BitString subjectUniqueID;
            Extensions extensions;

            constexpr bool parse(const asn1::TLV& tlv) {
                if (tlv.tag != asn1::Tag::SEQUENCE) {
                    return false;
                }
                size_t next = 0;
                return tlv.iterate(1, [&](const asn1::TLV& tlv, size_t i, bool last) {
                    switch (next) {
                        case 0:
                            next = 1;
                            if (tlv.tag == asn1::ctx_spec(0, true)) {
                                auto [t, ok1] = tlv.explicit_unstruct();
                                if (!ok1) {
                                    return false;
                                }
                                asn1::Integer tmp;
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
                            if (tlv.tag == asn1::ctx_spec(1)) {
                                return issuerUniqueID.parse(tlv, asn1::ctx_spec(1));
                            }
                            [[fallthrough]];
                        case 8:
                            next = 9;
                            if (tlv.tag == asn1::ctx_spec(2)) {
                                return subjectUniqueID.parse(tlv, asn1::ctx_spec(2));
                            }
                            [[fallthrough]];
                        case 9:
                            next = 10;
                            if (tlv.tag == asn1::ctx_spec(3, true)) {
                                return extensions.parse(tlv, asn1::ctx_spec(3, true)) && last;
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
            asn1::BitString signature;

            constexpr bool parse(const asn1::TLV& tlv) {
                if (tlv.tag != asn1::Tag::SEQUENCE) {
                    return false;
                }
                return tlv.iterate(3, [&](const asn1::TLV& tlv, size_t i, bool last) {
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
    }  // namespace fnet::x509::parser
}  // namespace futils
