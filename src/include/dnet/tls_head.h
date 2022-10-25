/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// tls_head - tls header
#pragma once
#include <cstdint>
#include "byte.h"

namespace utils {
    namespace dnet {
        namespace tls {

            using ProtocolVersion = std::uint16_t;
            template <class Len, class T, size_t minlen, size_t maxlen>
            struct TLSVec {
                Len len;
                T data[maxlen / sizeof(T)];
            };

            template <class Len, class T, size_t minlen, size_t maxlen>
            struct TLSLargeVec {
                Len len;
                T* data;
            };

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
                    const byte* fragment;
                };

                struct TLSInnerPlaintext {
                    const byte* content;
                    ContentType type;
                    std::uint32_t zeros_length_of_padding;
                };

                struct TLSCiphertext {
                    ContentType opaque_type = application_data;     /* 23 */
                    ProtocolVersion legacy_record_version = 0x0303; /* TLS v1.2 */
                    std::uint16_t length;
                    const byte* encrypted_record;
                };

                template <class Byte>
                bool enough_to_parse_record(const Byte* input, size_t len, size_t* expect_len) {
                    static_assert(sizeof(Byte) == 1, "unexpected input");
                    if (!input || !len) {
                        return false;
                    }
                    const byte* red = reinterpret_cast<const byte*>(input);
                    // any way we have to know
                    // how many byte we need to parse a record
                    if (len < 5) {
                        return false;
                    }
                    std::uint16_t payloadlen = std::uint16_t(red[3]) << 8 | red[4];
                    if (expect_len) {
                        *expect_len = payloadlen + 5;
                    }
                    if (payloadlen + 5 < len) {
                        return false;
                    }
                    return true;
                }

                template <class Byte>
                bool parse_record(const Byte* input, size_t len, auto&& cb) {
                    size_t expect_len = 0;
                    if (!enough_to_parse_record(input, len, &expect_len)) {
                        return false;
                    }
                    const byte* red = reinterpret_cast<const byte*>(input);
                    // yes we can't parse text so use plain
                    TLSPlaintext plain;
                    plain.type = ContentType(red[0]);
                    plain.legacy_record_version = std::uint16_t(red[1]) << 8 | red[2];
                    plain.length = std::uint16_t(red[3]) << 8 | red[4];
                    plain.fragment = red + 5;
                    cb(plain);
                    return true;
                }

            }  // namespace record

            namespace handshake {
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

                struct Handshake {
                    const HandshakeType msg_type; /* handshake type */
                    std::uint32_t length;         /* remaining bytes in message */
                    constexpr Handshake(HandshakeType typ)
                        : msg_type(typ) {}

                    const byte* raw_byte;
                };

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
                };

                struct Extension {
                    ExtensionType extension_type;
                    TLSLargeVec<std::uint16_t, byte, 0, 0xffff> extension_data;
                };

                using CipherSuite = std::uint16_t;

                struct ClientHello : Handshake {
                    constexpr ClientHello()
                        : Handshake(client_hello) {}
                    ProtocolVersion legacy_version = 0x0303; /* TLS v1.2 */
                    byte random[32];
                    TLSVec<byte, byte, 0, 32> legacy_session_id;
                    TLSLargeVec<std::uint16_t, CipherSuite, 2, 0xfffe> cipher_suites;
                    TLSLargeVec<byte, byte, 1, 254> legacy_compression_methods;
                    TLSLargeVec<std::uint16_t, Extension, 8, 0xffff> extensions;
                };

                struct ServerHello : Handshake {
                    constexpr ServerHello()
                        : Handshake(server_hello) {}
                    ProtocolVersion legacy_version = 0x0303; /* TLS v1.2 */
                    byte random[32];
                    TLSVec<byte, byte, 0, 32> legacy_session_id_echo;
                    CipherSuite cipher_suite;
                    byte legacy_compression_method = 0;
                    TLSLargeVec<std::uint16_t, Extension, 6, 0xffff> extensions;
                };

                struct ClientSupportedVersion {
                    TLSVec<byte, ProtocolVersion, 2, 254> versions;
                };

                struct ServerSupportedVersion {
                    ProtocolVersion selected_version;
                };

                struct Cookie {
                    TLSLargeVec<byte, byte, 1, 0xffff> cookie;
                };

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

                struct SignatureSchemeList {
                    TLSLargeVec<std::uint16_t, SignatureScheme, 2, 0xfffe> supported_signature_algorithms;
                };

                struct CertificateAuthoritiesExtension {
                    TLSLargeVec<std::uint16_t, byte, 3, 0xffff> authorities;
                };

                struct OIDFilter {
                    TLSLargeVec<byte, byte, 1, 0xff> certificate_extension_oid;
                    TLSLargeVec<byte, byte, 0, 0xffff> certificate_extension_values;
                };

                struct OIDFilterExtension {
                    TLSLargeVec<byte, OIDFilter, 0, 0xffff> filters;
                };

                struct PostHandshakeAuth {};

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
                    TLSLargeVec<std::uint16_t, NamedGroup, 2, 0xffff> named_group_list;
                };

                struct KeyShareEntry {
                    NamedGroup group;
                    TLSLargeVec<std::uint16_t, byte, 1, 0xffff> key_exchange;
                };

                struct KeyShareClientHello {
                    TLSLargeVec<std::uint16_t, KeyShareEntry, 1, 0xffff> client_shares;
                };

                struct KeyShareHelloRetryRequest {
                    NamedGroup selected_group;
                };

                struct KeyShareServerHello {
                    KeyShareEntry server_share;
                };

                template <class Len, class T, size_t minlen, size_t maxlen>
                using TLSFixedVec = TLSLargeVec<byte, byte, minlen, maxlen>;

                struct UncompressedPointRepresentation {
                    byte legacy_form = 4;
                    TLSFixedVec<byte, byte, 32, 66> X;
                    TLSFixedVec<byte, byte, 32, 66> Y;
                };

                enum PskKeyExchangeMode : byte {
                    psk_ke = 0,
                    psk_dhe_ke = 1,
                };

                struct PskKeyExchangeModes {
                    TLSVec<byte, PskKeyExchangeMode, 1, 255> ke_modes;
                };

                struct EarlyDataIndication {
                    std::uint32_t max_early_data_size;
                };

                struct PskIdentity {
                    TLSLargeVec<std::uint16_t, byte, 1, 0xffff> identity;
                    std::uint32_t obfuscated_ticket_age;
                };

                using PskBinderEntry = TLSLargeVec<byte, byte, 32, 255>;

                struct OfferedPsks {
                    TLSLargeVec<byte, PskIdentity, 7, 0xffff> identities;
                    TLSLargeVec<byte, PskIdentity, 33, 0xffff> binders;
                };

                struct ClientPreSharedKeyExtension {
                    OfferedPsks psks;
                };

                struct ServerPreSharedKeyExtension {
                    std::uint16_t selected_identity;
                };

                struct EncryptedExtensions : Handshake {
                    constexpr EncryptedExtensions()
                        : Handshake(encrypted_extensions) {}
                    TLSLargeVec<byte, Extension, 0, 0xffff> extensions;
                };

                struct CertificateRequest : Handshake {
                    constexpr CertificateRequest()
                        : Handshake(certificate_request) {}
                    TLSVec<byte, byte, 0, 255> certificate_request_context;
                    TLSLargeVec<byte, Extension, 2, 0xffff> extensions;
                };

                enum CertificateType : byte {
                    X509 = 0,
                    RawPublicKey = 2,
                };

                struct CertificateEntry {
                    TLSLargeVec<std::uint32_t, byte, 1, 0xffffff> cert_data;
                    TLSLargeVec<std::uint16_t, Extension, 0, 0xffff> extensions;
                };

                struct Certificate : Handshake {
                    constexpr Certificate()
                        : Handshake(certificate) {}
                    TLSVec<byte, byte, 0, 255> certificate_request_context;
                    TLSLargeVec<std::uint32_t, CertificateEntry, 0, 0xffffff> certificate_list;
                };

                struct CertificateVerify : Handshake {
                    constexpr CertificateVerify()
                        : Handshake(certificate_verify) {}
                    SignatureScheme algorithm;
                    TLSLargeVec<std::uint16_t, Extension, 0, 0xffff> signature;
                };

                struct Finished : Handshake {
                    constexpr Finished()
                        : Handshake(finished) {}
                    TLSFixedVec<std::uint16_t, byte, 12, 0xffff> verify_data;
                };

                struct EndOfEarlyData : Handshake {
                    constexpr EndOfEarlyData()
                        : Handshake(end_of_early_data) {}
                };

                struct NewSessionTicket {
                    std::uint32_t ticket_lifetime;
                    std::uint32_t ticket_age_add;
                    TLSVec<byte, byte, 0, 255> ticket_nonce;
                    TLSLargeVec<std::uint16_t, byte, 1, 0xffff> ticket;
                    TLSLargeVec<byte, Extension, 0, 0xffff> extensions;
                };

                enum KeyUpdateRequest : byte {
                    update_not_requested = 0,
                    update_requested = 1,
                };

                struct KeyUpdate {
                    KeyUpdateRequest request_update;
                };

                bool read_handshake(const record::TLSPlaintext& plain, auto&& cb) {
                    if (plain.type == record::handshake) {
                        if (plain.length < 4 || !plain.fragment) {
                            return false;
                        }
                        auto type = HandshakeType(plain.fragment[0]);
                        auto length = std::uint32_t(plain.fragment[1]) << 16 | std::uint32_t(plain.fragment[2]) << 8 | plain.fragment[3];
                        if (plain.length - 4 != length) {
                            return false;
                        }
                        Handshake h;
                        h.msg_type = type;
                        h.length = length;
                        h.raw_byte = plain.fragment + 4;
                        cb(h);
                        return true;
                    }
                    return false;
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
                };

                bool read_alert(const record::TLSPlaintext& plain, auto&& cb) {
                    if (plain.type == record::alert) {
                        if (plain.length < 2 || !plain.fragment) {
                            return false;
                        }
                        Alert alert;
                        alert.level = plain.fragment[0];
                        alert.description = plain.fragment[1];
                        cb(alert);
                    }
                    return false;
                }
            }  // namespace alert

            namespace key_schedule {
                struct HkdfLabel {
                    std::uint16_t length;
                    TLSVec<byte, byte, 7, 255> label{7, {'t', 'l', 's', '1', '3', ' '}};
                    TLSVec<byte, byte, 0, 255> context;
                };
            }  // namespace key_schedule

        }  // namespace tls
    }      // namespace dnet
}  // namespace utils
