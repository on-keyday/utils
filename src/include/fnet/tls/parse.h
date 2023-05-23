/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// parse - tls parser
// because this is made for QUIC crypto message parsing, parse only TLS1.3
#pragma once
#include <cstdint>
#include "../../core/byte.h"
#include "../../io/number.h"
#include "../quic/transport_parameter/transport_param.h"

namespace utils {
    namespace fnet::tls {

        using ProtocolVersion = std::uint16_t;

        struct uint24 {
            std::uint32_t value = 0;
            constexpr uint24() = default;
            constexpr uint24(std::uint32_t val)
                : value(val) {
                if (val > 0xffffff) {
                    if (std::is_constant_evaluated()) {
                        throw "too large number";
                    }
                    else {
                        abort();
                    }
                }
            }

            constexpr operator std::uint32_t() const {
                return value;
            }
        };

        namespace internal {
            template <size_t minlen, size_t maxlen>
            constexpr auto decide_int_type() {
                if constexpr (minlen == maxlen) {
                    return;
                }
                if constexpr (maxlen <= 0xFF) {
                    return std::uint8_t();
                }
                else if constexpr (maxlen <= 0xFFFF) {
                    return std::uint16_t();
                }
                else if constexpr (maxlen <= 0xFFFFFF) {
                    return uint24();
                }
                else if constexpr (maxlen <= 0xFFFFFFFF) {
                    return std::uint32_t();
                }
                else {
                    return std::uint64_t();
                }
            }
        }  // namespace internal

        template <class T, size_t minlen, size_t maxlen = minlen>
        struct NumVector {
            using LenFieldT = decltype(internal::decide_int_type<minlen, maxlen>());

           private:
            static_assert(minlen <= maxlen);
            view::rvec data_;

            constexpr bool read_data(io::reader& r, std::uint64_t len) {
                if (len < minlen || maxlen < len) {
                    return false;
                }
                if (!r.read(data_, len)) {
                    return false;
                }
                return valid();
            }

           public:
            constexpr NumVector() = default;

            constexpr bool parse(io::reader& r) {
                if constexpr (minlen == maxlen) {
                    return read_data(r, maxlen);  // no length field
                }
                else if constexpr (std::is_same_v<LenFieldT, uint24>) {
                    std::uint32_t len = 0;
                    if (!io::read_uint24(r, len)) {
                        return false;
                    }
                    return read_data(r, len);
                }
                else {
                    LenFieldT len = 0;
                    if (!io::read_num(r, len)) {
                        return false;
                    }
                    return read_data(r, len);
                }
            }

            constexpr bool valid() const {
                const auto s = data_.size();
                return minlen <= s && s <= maxlen && s % sizeof(T) == 0;
            }

            constexpr size_t size() const {
                if (!valid()) {
                    return 0;
                }
                return data_.size() / sizeof(T);
            }

            constexpr T operator[](size_t i) const {
                const auto index = i * sizeof(T);
                if (index >= size()) {
                    return T{};
                }
                return endian::read_from<T>(data_.data() + index, true);
            }

            constexpr view::rvec data() const {
                return data_;
            }
        };

        template <class T, size_t minlen, size_t maxlen>
        struct ElementVector {
           private:
            view::rvec data_;

           public:
            constexpr bool iterate(auto&& cb) const noexcept {
                io::reader r{data_};
                T element;
                while (!r.empty()) {
                    if (!element.parse(r)) {
                        return false;
                    }
                    if (!cb(element, r.empty())) {
                        return false;
                    }
                }
                return true;
            }

            constexpr bool parse(io::reader& r) noexcept {
                NumVector<byte, minlen, maxlen> tmp;
                if (!tmp.parse(r)) {
                    return false;
                }
                data_ = tmp.data();
                return iterate([](auto...) { return true; });
            }

            constexpr view::rvec data() const noexcept {
                return data_;
            }
        };

        constexpr std::uint16_t legacy_version = 0x303;

        namespace record {
            enum ContentType : byte {
                invalid = 0,
                change_cipher_spec = 20,
                alert = 21,
                handshake = 22,
                application_data = 23,
            };

            struct TLSPlaintext {
                ContentType type;
                ProtocolVersion legacy_record_version;
                std::uint16_t length;
                view::rvec fragment;

                constexpr bool parse(io::reader& r) {
                    return io::read_num(r, type) &&
                           io::read_num(r, legacy_record_version) &&
                           io::read_num(r, length) &&
                           r.read(fragment, length);
                }
            };

            struct TLSInnerPlaintext {
                view::rvec content;
                ContentType type;
                std::uint32_t zeros_length_of_padding;
            };

            struct TLSCiphertext {
                const ContentType opaque_type = application_data;             /* 23 */
                const ProtocolVersion legacy_record_version = legacy_version; /* TLS v1.2 */
                std::uint16_t length;
                view::rvec encrypted_text;

                bool parse(io::reader& r) {
                    TLSPlaintext plain;
                    if (!plain.parse(r)) {
                        return false;
                    }
                    if (plain.type != opaque_type ||
                        plain.legacy_record_version != legacy_record_version) {
                        return false;
                    }
                    length = plain.length;
                    encrypted_text = plain.fragment;
                    return true;
                }
            };

        }  // namespace record

        namespace handshake {

            enum SignatureScheme : std::uint16_t {
                /* RSASSA-PKCS1-v1_5 algorithms */
                rsa_pkcs1_sha256 = 0x0401,
                rsa_pkcs1_sha384 = 0x0501,
                rsa_pkcs1_sha512 = 0x0601,

                /* ECDSA algorithms */
                ecdsa_secp256r1_sha256 = 0x0403,
                ecdsa_secp384r1_sha384 = 0x0503,
                ecdsa_secp521r1_sha512 = 0x0603,

                /* RSASSA-PSS algorithms with public key OID rsaEncryption */
                rsa_pss_rsae_sha256 = 0x0804,
                rsa_pss_rsae_sha384 = 0x0805,
                rsa_pss_rsae_sha512 = 0x0806,

                /* EdDSA algorithms */
                ed25519 = 0x0807,
                ed448 = 0x0808,

                /* RSASSA-PSS algorithms with public key OID RSASSA-PSS */
                rsa_pss_pss_sha256 = 0x0809,
                rsa_pss_pss_sha384 = 0x080a,
                rsa_pss_pss_sha512 = 0x080b,

                /* Legacy algorithms */
                rsa_pkcs1_sha1 = 0x0201,
                ecdsa_sha1 = 0x0203,

                /* Reserved Code Points */
                private_use_start = 0xFE00,

            };

            enum HandshakeType : byte {
                client_hello = 1,
                server_hello = 2,
                new_session_ticket = 4,
                end_of_early_data = 5,
                encrypted_extensions = 8,
                certificate = 11,
                certificate_request = 13,
                certificate_verify = 15,
                finished = 20,
                key_update = 24,
                message_hash = 254,
            };

            constexpr const char* to_string(HandshakeType type) noexcept {
                switch (type) {
#define MAP(code)             \
    case HandshakeType::code: \
        return #code;
                    MAP(client_hello)
                    MAP(server_hello)
                    MAP(new_session_ticket)
                    MAP(end_of_early_data)
                    MAP(encrypted_extensions)
                    MAP(certificate)
                    MAP(certificate_request)
                    MAP(certificate_verify)
                    MAP(finished)
                    MAP(key_update)
                    MAP(message_hash)
                    default:
                        return nullptr;
#undef MAP
                }
            }

            namespace ext {

                enum ExtensionType : std::uint16_t {
                    server_name = 0,                             /* RFC 6066 */
                    max_fragment_length = 1,                     /* RFC 6066 */
                    status_request = 5,                          /* RFC 6066 */
                    supported_groups = 10,                       /* RFC 8422, 7919 */
                    signature_algorithms = 13,                   /* RFC 8446 */
                    use_srtp = 14,                               /* RFC 5764 */
                    heartbeat = 15,                              /* RFC 6520 */
                    application_layer_protocol_negotiation = 16, /* RFC 7301 */
                    signed_certificate_timestamp = 18,           /* RFC 6962 */
                    client_certificate_type = 19,                /* RFC 7250 */
                    server_certificate_type = 20,                /* RFC 7250 */
                    padding = 21,                                /* RFC 7685 */
                    pre_shared_key = 41,                         /* RFC 8446 */
                    early_data = 42,                             /* RFC 8446 */
                    supported_versions = 43,                     /* RFC 8446 */
                    cookie = 44,                                 /* RFC 8446 */
                    psk_key_exchange_modes = 45,                 /* RFC 8446 */
                    certificate_authorities = 47,                /* RFC 8446 */
                    oid_filters = 48,                            /* RFC 8446 */
                    post_handshake_auth = 49,                    /* RFC 8446 */
                    signature_algorithms_cert = 50,              /* RFC 8446 */
                    key_share = 51,                              /* RFC 8446 */
                    quic_transport_parameters = 0x39,            /* RFC 9001 */
                };

                constexpr const char* to_string(ExtensionType type) noexcept {
#define MAP(code)             \
    case ExtensionType::code: \
        return #code;
                    switch (type) {
                        MAP(server_name)
                        MAP(max_fragment_length)
                        MAP(status_request)
                        MAP(supported_groups)
                        MAP(signature_algorithms)
                        MAP(use_srtp)
                        MAP(heartbeat)
                        MAP(application_layer_protocol_negotiation)
                        MAP(signed_certificate_timestamp)
                        MAP(client_certificate_type)
                        MAP(server_certificate_type)
                        MAP(padding)
                        MAP(pre_shared_key)
                        MAP(early_data)
                        MAP(supported_versions)
                        MAP(cookie)
                        MAP(psk_key_exchange_modes)
                        MAP(certificate_authorities)
                        MAP(oid_filters)
                        MAP(post_handshake_auth)
                        MAP(signature_algorithms_cert)
                        MAP(key_share)
                        MAP(quic_transport_parameters)
                        default:
                            return nullptr;
                    }
#undef MAP
                }

                struct Extension {
                    ExtensionType extension_type{};
                    NumVector<byte, 0, 0xffff> extension_data;

                    constexpr bool parse(io::reader& r) {
                        return io::read_num(r, extension_type) &&
                               extension_data.parse(r);
                    }

                    constexpr view::rvec data() const {
                        return extension_data.data();
                    }

                    constexpr io::reader reader() const {
                        return io::reader{data()};
                    }
                };

                struct ClientSupportedVersion {
                    NumVector<ProtocolVersion, 2, 254> versions;

                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::supported_versions) {
                            return false;
                        }
                        auto r = ext.reader();
                        return versions.parse(r) && r.empty();
                    }
                };

                struct ServerSupportedVersion {
                    ProtocolVersion selected_version;

                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::supported_versions) {
                            return false;
                        }
                        auto r = ext.reader();
                        return io::read_num(r, selected_version) && r.empty();
                    }
                };

                struct Cookie {
                    NumVector<byte, 1, 0xffff> cookie;

                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::cookie) {
                            return false;
                        }
                        auto r = ext.reader();
                        return cookie.parse(r) && r.empty();
                    }
                };

                struct SignatureSchemeList {
                    NumVector<SignatureScheme, 2, 0xfffe> supported_signature_algorithms;

                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::signature_algorithms &&
                            ext.extension_type != ExtensionType::signature_algorithms_cert) {
                            return false;
                        }
                        auto r = ext.reader();
                        return supported_signature_algorithms.parse(r) && r.empty();
                    }
                };

                using DistinguishedName = NumVector<byte, 1, 0xffff>;

                struct CertificateAuthoritiesExtension {
                    ElementVector<DistinguishedName, 3, 0xffff> authorities;

                    constexpr bool parse(const Extension& ext) {
                        if (ext.extension_type != ExtensionType::certificate_authorities) {
                            return false;
                        }
                        auto r = ext.reader();
                        return authorities.parse(r) && r.empty();
                    }
                };

                struct OIDFilter {
                    NumVector<byte, 1, 0xff> oid;
                    NumVector<byte, 0, 0xffff> values;

                    constexpr bool parse(io::reader& r) noexcept {
                        return oid.parse(r) &&
                               values.parse(r);
                    }
                };

                struct OIDFilterExtension {
                    ElementVector<OIDFilter, 0, 0xffff> filters;

                    constexpr bool parse(const Extension& ext) {
                        if (ext.extension_type != ExtensionType::oid_filters) {
                            return false;
                        }
                        auto r = ext.reader();
                        return filters.parse(r) && r.empty();
                    }
                };

                struct PostHandshakeAuth {
                    constexpr bool parse(const Extension& ext) {
                        if (ext.extension_type != ExtensionType::post_handshake_auth) {
                            return false;
                        }
                        return ext.data().empty();
                    }
                };

                enum NamedGroup : std::uint16_t {

                    /* Elliptic Curve Groups (ECDHE) */
                    secp256r1 = 0x0017,
                    secp384r1 = 0x0018,
                    secp521r1 = 0x0019,
                    x25519 = 0x001D,
                    x448 = 0x001E,

                    /* Finite Field Groups (DHE) */
                    ffdhe2048 = 0x0100,
                    ffdhe3072 = 0x0101,
                    ffdhe4096 = 0x0102,
                    ffdhe6144 = 0x0103,
                    ffdhe8192 = 0x0104,

                    /* Reserved Code Points */
                    ffdhe_private_use_begin = 0x01FC,
                    ffdhe_private_use_end = 0x01FF,
                    ecdhe_private_use_begin = 0xFE00,
                    ecdhe_private_use_begin_end = 0xFEFF,

                };

                struct NamedGroupList {
                    NumVector<NamedGroup, 2, 0xffff> named_group_list;
                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::post_handshake_auth) {
                            return false;
                        }
                        return ext.data().empty();
                    }
                };

                struct KeyShareEntry {
                    NamedGroup group;
                    NumVector<byte, 1, 0xffff> key_exchange;
                    constexpr bool parse(io::reader& r) noexcept {
                        return io::read_num(r, group) &&
                               key_exchange.parse(r);
                    }
                };

                struct KeyShareClientHello {
                    ElementVector<KeyShareEntry, 0, 0xffff> client_shares;
                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::key_share) {
                            return false;
                        }
                        auto r = ext.reader();
                        return client_shares.parse(r) && r.empty();
                    }
                };

                struct KeyShareHelloRetryRequest {
                    NamedGroup selected_group;
                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::key_share) {
                            return false;
                        }
                        auto r = ext.reader();
                        return io::read_num(r, selected_group) && r.empty();
                    }
                };

                struct KeyShareServerHello {
                    KeyShareEntry server_share;

                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::key_share) {
                            return false;
                        }
                        auto r = ext.reader();
                        return server_share.parse(r) && r.empty();
                    }
                };

                struct UncompressedPointRepresentation {
                    const byte legacy_form = 4;
                    byte X[66];
                    byte Y[66];

                    constexpr bool parse(const KeyShareEntry& entry) noexcept {
                        if (entry.group != NamedGroup::secp256r1 &&
                            entry.group != NamedGroup::secp384r1 &&
                            entry.group != NamedGroup::secp521r1) {
                            return false;
                        }
                        if (!entry.key_exchange.valid()) {
                            return false;
                        }
                        io::reader r{entry.key_exchange.data()};
                        byte form;
                        if (!r.read(view::wvec(&form, 1)) ||
                            form != legacy_form) {
                            return false;
                        }
                        switch (entry.group) {
                            case NamedGroup::secp256r1:
                                return r.read(view::wvec(X, 32)) &&
                                       r.read(view::wvec(Y, 32)) &&
                                       r.empty();
                            case NamedGroup::secp384r1:
                                return r.read(view::wvec(X, 48)) &&
                                       r.read(view::wvec(Y, 48)) &&
                                       r.empty();
                            case NamedGroup::secp521r1:
                                return r.read(view::wvec(X, 66)) &&
                                       r.read(view::wvec(Y, 66)) &&
                                       r.empty();
                            default:
                                return false;
                        }
                    }
                };

                enum PskKeyExchangeMode : byte {
                    psk_ke = 0,
                    psk_dhe_ke = 1,
                };

                struct PskKeyExchangeModes {
                    NumVector<PskKeyExchangeMode, 1, 255> ke_modes;

                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::psk_key_exchange_modes) {
                            return false;
                        }
                        auto r = ext.reader();
                        return ke_modes.parse(r) && r.empty();
                    }
                };

                struct EarlyDataIndication {
                    std::uint32_t max_early_data_size;
                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::early_data) {
                            return false;
                        }
                        auto r = ext.reader();
                        return io::read_num(r, max_early_data_size) && r.empty();
                    }
                };

                struct EmptyEarlyDataIndication {
                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::early_data) {
                            return false;
                        }
                        return ext.data().empty();
                    }
                };

                struct PskIdentity {
                    NumVector<byte, 1, 0xffff> identity;
                    std::uint32_t obfuscated_ticket_age;

                    constexpr bool parse(io::reader& r) noexcept {
                        return identity.parse(r) &&
                               io::read_num(r, obfuscated_ticket_age);
                    }
                };

                using PskBinderEntry = NumVector<byte, 32, 255>;

                struct OfferedPsks {
                    ElementVector<PskIdentity, 7, 0xffff> identities;
                    ElementVector<PskBinderEntry, 33, 0xffff> binders;

                    constexpr bool parse(io::reader& r) noexcept {
                        return identities.parse(r) &&
                               binders.parse(r);
                    }
                };

                struct ClientPreSharedKeyExtension {
                    OfferedPsks psks;

                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::pre_shared_key) {
                            return false;
                        }
                        auto r = ext.reader();
                        return psks.parse(r) && r.empty();
                    }
                };

                struct ServerPreSharedKeyExtension {
                    std::uint16_t selected_identity;

                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::pre_shared_key) {
                            return false;
                        }
                        auto r = ext.reader();
                        return io::read_num(r, selected_identity) && r.empty();
                    }
                };

                using ProtocolName = NumVector<byte, 1, 0xff>;

                struct ProtocolNameList {
                    ElementVector<ProtocolName, 2, 0xffff> protocol_name_list;

                    constexpr bool parse(const Extension& ext) noexcept {
                        if (ext.extension_type != ExtensionType::application_layer_protocol_negotiation) {
                            return false;
                        }
                        auto r = ext.reader();
                        return protocol_name_list.parse(r) && r.empty();
                    }
                };

                struct TransportParamVector {
                   private:
                    view::rvec data;

                   public:
                    constexpr bool iterate(auto&& cb) const {
                        io::reader r{data};
                        while (!r.empty()) {
                            quic::trsparam::TransportParameter param;
                            if (!param.parse(r)) {
                                return false;
                            }
                            if (!cb(param, r.empty())) {
                                return false;
                            }
                        }
                        return true;
                    }

                    constexpr bool parse(view::rvec in) {
                        data = in;
                        return iterate([](auto...) { return true; });
                    }
                };

                struct QUICTransportParamExtension {
                    TransportParamVector params;
                    constexpr bool parse(const Extension& ext) {
                        if (ext.extension_type != ExtensionType::quic_transport_parameters) {
                            return false;
                        }
                        return params.parse(ext.extension_data.data());
                    }
                };

                constexpr bool parse(const Extension& ext, HandshakeType hs, auto&& cb) {
#define PARSE(Type)          \
    {                        \
        Type t;              \
        if (!t.parse(ext)) { \
            cb(t, true);     \
            return false;    \
        }                    \
        return cb(t, false); \
    }
#define DEFAULT        \
    {                  \
        cb(ext, true); \
        return false;  \
    }
                    switch (ext.extension_type) {
                        case ExtensionType::supported_versions:
                            switch (hs) {
                                case HandshakeType::client_hello:
                                    PARSE(ClientSupportedVersion)
                                case HandshakeType::server_hello:
                                    PARSE(ServerSupportedVersion)
                                default:
                                    DEFAULT
                            }
                        case ExtensionType::cookie:
                            PARSE(Cookie)
                        case ExtensionType::signature_algorithms:
                        case ExtensionType::signature_algorithms_cert:
                            PARSE(SignatureSchemeList)
                        case ExtensionType::certificate_authorities:
                            PARSE(CertificateAuthoritiesExtension)
                        case ExtensionType::oid_filters:
                            PARSE(OIDFilterExtension)
                        case ExtensionType::supported_groups:
                            PARSE(NamedGroupList)
                        case ExtensionType::key_share:
                            switch (hs) {
                                case HandshakeType::client_hello:
                                    PARSE(KeyShareClientHello)
                                case HandshakeType::server_hello: {
                                    KeyShareHelloRetryRequest retry;
                                    if (retry.parse(ext)) {
                                        return cb(retry, false);
                                    }
                                    PARSE(KeyShareServerHello)
                                    default:
                                        DEFAULT
                                }
                            }
                        case ExtensionType::psk_key_exchange_modes:
                            PARSE(PskKeyExchangeModes)
                        case ExtensionType::early_data:
                            switch (hs) {
                                case HandshakeType::new_session_ticket:
                                    PARSE(EarlyDataIndication)
                                case HandshakeType::client_hello:
                                case HandshakeType::encrypted_extensions:
                                    PARSE(EmptyEarlyDataIndication)
                                default:
                                    DEFAULT
                            }
                        case ExtensionType::pre_shared_key:
                            switch (hs) {
                                case HandshakeType::client_hello:
                                    PARSE(ClientPreSharedKeyExtension)
                                case HandshakeType::server_hello:
                                    PARSE(ServerPreSharedKeyExtension)
                                default:
                                    DEFAULT
                            }
                        case ExtensionType::application_layer_protocol_negotiation:
                            PARSE(ProtocolNameList)
                        case ExtensionType::quic_transport_parameters:
                            PARSE(QUICTransportParamExtension)
                        default:
                            DEFAULT
                    }
#undef PARSE
#undef DEFAULT
                }

            }  // namespace ext

            struct Handshake {
                const HandshakeType msg_type; /* handshake type */
                uint24 length;                /* remaining bytes in message */
                constexpr Handshake(HandshakeType typ)
                    : msg_type(typ) {}

               protected:
                constexpr std::pair<view::rvec, bool> parse_unknown(io::reader& r) {
                    HandshakeType type;
                    std::uint32_t value;
                    if (!io::read_num(r, type) ||
                        !io::read_uint24(r, value)) {
                        return {{view::rvec{}}, false};
                    }
                    const_cast<HandshakeType&>(msg_type) = type;
                    length = value;
                    auto [data, ok] = r.read(length);
                    return {data, ok};
                }

               public:
                constexpr std::pair<io::reader, bool> parse_msg(io::reader& r) {
                    HandshakeType type;
                    std::uint32_t value;
                    auto p = r.peeker();
                    if (!io::read_num(p, type) ||
                        type != msg_type ||
                        !io::read_uint24(p, value)) {
                        return {{view::rvec{}}, false};
                    }
                    length = value;
                    r = p;
                    auto [data, ok] = r.read(length);
                    return {data, ok};
                }
            };

            enum class CipherSuite : std::uint16_t {
                TLS_NULL_WITH_NULL_NULL = 0x0000,
                TLS_AES_128_GCM_SHA256 = 0x1301,
                TLS_AES_256_GCM_SHA384 = 0x1302,
                TLS_CHACHA20_POLY1305_SHA256 = 0x1303,
                TLS_AES_128_CCM_SHA256 = 0x1304,
                TLS_AES_128_CCM_8_SHA256 = 0x1305,
            };

            constexpr const char* to_string(CipherSuite suite) noexcept {
                switch (suite) {
#define MAP(code)           \
    case CipherSuite::code: \
        return #code;
                    MAP(TLS_NULL_WITH_NULL_NULL)
                    MAP(TLS_AES_128_GCM_SHA256)
                    MAP(TLS_AES_256_GCM_SHA384)
                    MAP(TLS_CHACHA20_POLY1305_SHA256)
                    MAP(TLS_AES_128_CCM_SHA256)
                    MAP(TLS_AES_128_CCM_8_SHA256)
                    default:
                        return nullptr;
#undef MAP
                }
            }

            struct ClientHello : Handshake {
                constexpr ClientHello()
                    : Handshake(client_hello) {}
                const ProtocolVersion legacy_version = tls::legacy_version; /* TLS v1.2 */
                byte random[32];
                NumVector<byte, 0, 32> legacy_session_id;
                NumVector<CipherSuite, 2, 0xfffe> cipher_suites;
                NumVector<byte, 1, 254> legacy_compression_methods;
                ElementVector<ext::Extension, 8, 0xffff> extensions;

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    ProtocolVersion version;
                    return io::read_num(sub, version) &&
                           version == legacy_version &&
                           sub.read(random) &&
                           legacy_session_id.parse(sub) &&
                           cipher_suites.parse(sub) &&
                           legacy_compression_methods.parse(sub) &&
                           extensions.parse(sub) &&
                           sub.empty();
                }
            };

            struct ServerHello : Handshake {
                constexpr ServerHello()
                    : Handshake(server_hello) {}
                const ProtocolVersion legacy_version = tls::legacy_version; /* TLS v1.2 */
                byte random[32];
                NumVector<byte, 0, 32> legacy_session_id_echo;
                CipherSuite cipher_suite;
                const byte legacy_compression_method = 0;
                ElementVector<ext::Extension, 6, 0xffff> extensions;

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    ProtocolVersion version;
                    byte meth;
                    return io::read_num(sub, version) &&
                           version == legacy_version &&
                           sub.read(random) &&
                           legacy_session_id_echo.parse(sub) &&
                           io::read_num(sub, cipher_suite) &&
                           sub.read(view::wvec(&meth, 1)) &&
                           meth == legacy_compression_method &&
                           extensions.parse(sub) &&
                           sub.empty();
                }
            };

            struct EncryptedExtensions : Handshake {
                constexpr EncryptedExtensions()
                    : Handshake(encrypted_extensions) {}
                ElementVector<ext::Extension, 0, 0xffff> extensions;

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    return extensions.parse(sub) &&
                           sub.empty();
                }
            };

            struct CertificateRequest : Handshake {
                constexpr CertificateRequest()
                    : Handshake(certificate_request) {}
                NumVector<byte, 0, 255> certificate_request_context;
                ElementVector<ext::Extension, 2, 0xffff> extensions;
                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    return certificate_request_context.parse(sub) &&
                           extensions.parse(sub) &&
                           sub.empty();
                }
            };

            enum CertificateType : byte {
                X509 = 0,
                RawPublicKey = 2,
            };

            struct CertificateEntry {
                NumVector<byte, 1, 0xffffff> cert_data;
                ElementVector<ext::Extension, 0, 0xffff> extensions;

                constexpr bool parse(io::reader& r) noexcept {
                    return cert_data.parse(r) &&
                           extensions.parse(r);
                }
            };

            struct Certificate : Handshake {
                constexpr Certificate()
                    : Handshake(certificate) {}
                NumVector<byte, 0, 255> certificate_request_context;
                ElementVector<CertificateEntry, 0, 0xffffff> certificate_list;

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    return certificate_request_context.parse(sub) &&
                           certificate_list.parse(sub) &&
                           sub.empty();
                }
            };

            struct CertificateVerify : Handshake {
                constexpr CertificateVerify()
                    : Handshake(certificate_verify) {}
                SignatureScheme algorithm;
                NumVector<byte, 0, 0xffff> signature;

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    return io::read_num(sub, algorithm) &&
                           signature.parse(sub) &&
                           sub.empty();
                }
            };

            struct Finished : Handshake {
                constexpr Finished()
                    : Handshake(finished) {}
                view::rvec verify_data;

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    verify_data = sub.remain();
                    return true;
                }
            };

            struct EndOfEarlyData : Handshake {
                constexpr EndOfEarlyData()
                    : Handshake(end_of_early_data) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    return sub.empty();
                }
            };

            struct NewSessionTicket : Handshake {
                constexpr NewSessionTicket()
                    : Handshake(new_session_ticket) {}

                std::uint32_t ticket_lifetime;
                std::uint32_t ticket_age_add;
                NumVector<byte, 0, 255> ticket_nonce;
                NumVector<byte, 1, 0xffff> ticket;
                ElementVector<ext::Extension, 0, 0xffff> extensions;
                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    return io::read_num(sub, ticket_lifetime) &&
                           io::read_num(sub, ticket_age_add) &&
                           ticket_nonce.parse(sub) &&
                           ticket.parse(sub) &&
                           extensions.parse(sub) &&
                           sub.empty();
                }
            };

            enum KeyUpdateRequest : byte {
                update_not_requested = 0,
                update_requested = 1,
            };

            struct KeyUpdate : Handshake {
                constexpr KeyUpdate()
                    : Handshake(key_update) {}

                KeyUpdateRequest request_update;

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_msg(r);
                    if (!ok) {
                        return false;
                    }
                    return io::read_num(r, request_update) &&
                           sub.empty();
                }
            };

            struct Unknown : Handshake {
                view::rvec raw_data;

                constexpr Unknown()
                    : Handshake(server_hello) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_unknown(r);
                    if (!ok) {
                        return false;
                    }
                    raw_data = sub;
                    return true;
                }
            };

            constexpr bool parse_one(io::reader& r, auto&& cb) {
                if (r.empty()) {
                    return false;
                }
                switch (HandshakeType(r.top())) {
#define PARSE(msg, Type)       \
    case HandshakeType::msg: { \
        Type t;                \
        if (!t.parse(r)) {     \
            cb(t, true);       \
            return false;      \
        }                      \
        return cb(t, false);   \
    }
                    PARSE(client_hello, ClientHello)
                    PARSE(server_hello, ServerHello)
                    PARSE(encrypted_extensions, EncryptedExtensions)
                    PARSE(certificate, Certificate)
                    PARSE(certificate_request, CertificateRequest)
                    PARSE(certificate_verify, CertificateVerify)
                    PARSE(end_of_early_data, EndOfEarlyData)
                    PARSE(new_session_ticket, NewSessionTicket)
                    PARSE(key_update, KeyUpdate)
                    PARSE(finished, Finished)
                    default: {
                        Unknown t;
                        t.parse(r);
                        return cb(t, false);
                    }
                }
#undef PARSE
            }

        }  // namespace handshake

        namespace alert {
            enum AlertLevel : byte {
                warning = 1,
                fatal = 2,
            };

            enum AlertDescription : byte {
                close_notify = 0,
                unexpected_message = 10,
                bad_record_mac = 20,
                record_overflow = 22,
                handshake_failure = 40,
                bad_certificate = 42,
                unsupported_certificate = 43,
                certificate_revoked = 44,
                certificate_expired = 45,
                certificate_unknown = 46,
                illegal_parameter = 47,
                unknown_ca = 48,
                access_denied = 49,
                decode_error = 50,
                decrypt_error = 51,
                protocol_version = 70,
                insufficient_security = 71,
                internal_error = 80,
                inappropriate_fallback = 86,
                user_canceled = 90,
                missing_extension = 109,
                unsupported_extension = 110,
                unrecognized_name = 112,
                bad_certificate_status_response = 113,
                unknown_psk_identity = 115,
                certificate_required = 116,
                no_application_protocol = 120,
            };

            struct Alert {
                AlertLevel level;
                AlertDescription description;

                constexpr bool parse(io::reader& r) {
                    return io::read_num(r, level) &&
                           io::read_num(r, description);
                }
            };

        }  // namespace alert

        namespace key_schedule {
            struct HkdfLabel {
                std::uint16_t length;
                NumVector<byte, 7, 255> label;
                NumVector<byte, 0, 255> context;

                constexpr bool parse(io::reader& r) {
                    if (!io::read_num(r, length)) {
                        return false;
                    }
                    auto [sub, ok] = r.read(length);
                    if (!ok) {
                        return false;
                    }
                    io::reader subr{sub};
                    return label.parse(subr) &&
                           context.parse(subr) &&
                           subr.empty();
                }
            };
        }  // namespace key_schedule

    }  // namespace fnet::tls
}  // namespace utils
