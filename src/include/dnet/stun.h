/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stun - stun client implement
#pragma once
#include <cstdint>
#include "byte.h"
#include "bytelen.h"

namespace utils {
    namespace dnet {
        namespace stun {
            enum MessageType : std::uint16_t {
                msg_binding_request = 0x0001,
                msg_binding_response = 0x0101,
                msg_binding_error_response = 0x0111,
                msg_shared_secret_request = 0x002,
                msg_shared_secret_response = 0x0102,
                msg_shared_secret_error_response = 0x0112,
            };

            enum class NATType {
                udp_blocked,
                open_internet,
                full_cone,
                restricted_cone,
                port_restricted_cone,
                symmetric,
                udp_firewall,
                unknown,
            };

            struct StunHeader {
                std::uint16_t type;
                std::uint16_t length;
                const std::uint32_t magic_cookie = 0x2112A442;
                byte transaction_id[12];

                void pack(WPacket& w) const {
                    w.as(type);
                    w.as(length);
                    w.as(magic_cookie);
                    w.copy_from(ConstByteLen{transaction_id, 12});
                }

                bool unpack(ByteLen& b) {
                    std::uint32_t cookie = 0;
                    if (!(b.as(type) &&
                          (b = b.forward(2), b.as(length)) &&
                          (b = b.forward(2), b.as(cookie) && cookie == magic_cookie) &&
                          (b = b.forward(4), b.enough(12)))) {
                        return false;
                    }
                    memcpy(transaction_id, b.data, 12);
                    b = b.forward(12);
                    return true;
                }
            };

            struct Attribute {
                std::uint16_t type;
                std::uint16_t length;
                ByteLen value;
                bool pack(WPacket& w, bool enc_value = false) const {
                    if (enc_value) {
                        if (!value.enough(length)) {
                            return false;
                        }
                    }
                    w.as(type);
                    w.as(length);
                    if (enc_value) {
                        w.copy_from(value.resized(length));
                    }
                    return true;
                }

                bool unpack(ByteLen& b, bool dec_value = true) {
                    if (!((b.as(type) &&
                           (b = b.forward(2), b.as(length)) &&
                           (b = b.forward(2), b.enough(length))))) {
                        return false;
                    }
                    if (dec_value) {
                        value = b.resized(length);
                        b = b.forward(length);
                    }
                    return true;
                }
            };

            enum StunAddrFamily {
                family_ipv4 = 0x1,
                family_ipv6 = 0x2,
            };

            struct MappedAddress {
                byte family;
                std::uint16_t port;
                byte address[16];

                size_t len() const {
                    return family == 0x1 ? 8 : family == 2 ? 20
                                                           : 0;
                }

                friend bool operator==(const MappedAddress& a, const MappedAddress& b) {
                    if (a.family != b.family) {
                        return false;
                    }
                    if (a.family != 1 && a.family != 2) {
                        return false;
                    }
                    if (a.port != b.port) {
                        return false;
                    }
                    if (a.family == 1) {
                        return ConstByteLen{a.address, 4}.equal_to(ConstByteLen{b.address, 4});
                    }
                    else {
                        return ConstByteLen{a.address, 16}.equal_to(ConstByteLen{b.address, 16});
                    }
                }

                bool pack(WPacket& w, const byte* xor_ = nullptr) const {
                    if (family != 0x1 && family != 0x2) {
                        return false;
                    }
                    w.as<byte>(0);
                    w.as(family);
                    if (auto v = w.as(port); v.valid() && xor_) {
                        v.data[0] ^= xor_[0];
                        v.data[1] ^= xor_[1];
                    }
                    size_t len = family == 0x1 ? 4 : 16;
                    if (auto v = w.copy_from(ConstByteLen{address, len}); v.valid() && xor_) {
                        for (auto i = 0; i < len; i++) {
                            v.data[i] ^= xor_[i];
                        }
                    }
                    return true;
                }

                bool unpack(ByteLen& b, const byte* xor_ = nullptr) {
                    byte tmp;
                    if (!(b.as(tmp) && tmp == 0 &&
                          (b = b.forward(1),
                           b.as(family) && (family == 0x1 || family == 0x2)))) {
                        return false;
                    }
                    if (!(b = b.forward(1), b.enough(2))) {
                        return false;
                    }
                    if (xor_) {
                        b.data[0] ^= xor_[0];
                        b.data[1] ^= xor_[1];
                    }
                    port = b.as<std::uint16_t>();
                    auto len = family == 1 ? 4 : 16;
                    if (!(b = b.forward(2), b.enough(len))) {
                        return false;
                    }
                    if (xor_) {
                        for (auto i = 0; i < len; i++) {
                            b.data[i] ^= xor_[i];
                        }
                    }
                    b.copy_to(address, len);
                    b = b.forward(len);
                    return true;
                }
            };

            struct ChangeRequest {
                bool change_ip = false;
                bool change_port = false;
                void pack(WPacket& w) const {
                    std::uint32_t val = 0;
                    if (change_ip) {
                        val |= 0x4;
                    }
                    if (change_port) {
                        val |= 0x2;
                    }
                    w.as(val);
                }
                bool unpack(ByteLen& in) {
                    std::uint32_t val = 0;
                    if (!in.as(val)) {
                        return false;
                    }
                    in = in.forward(4);
                    change_ip = (val & 0x4) != 0;
                    change_port = (val & 0x2) != 0;
                    return true;
                }
            };

            struct ErrorCode {
                std::uint32_t code;
                bool pack(WPacket& w) const {
                    auto class_ = code / 100;
                    auto number = code % 100;
                    if (class_ < 3 || 6 < class_) {
                        return false;
                    }
                    // 10bit
                    w.as(std::uint32_t(class_ << 7 | number));
                    return true;
                }
                bool unpack(ByteLen& b) {
                    if (!b.as(code)) {
                        return false;
                    }
                    if (code & 0xFFFFFC00) {
                        return false;
                    }
                    auto class_ = (code >> 7) & 0x7;
                    auto number_ = code & 0x7F;
                    code = class_ * 100 + number_;
                    return false;
                }
            };

            enum AttributeType : std::uint16_t {
                attr_reserved = 0x0000,
                attr_mapped_address = 0x0001,
                attr_response_address = 0x0002,
                attr_change_request = 0x0003,
                attr_source_address = 0x0004,
                attr_changed_address = 0x0005,
                attr_user_name = 0x0006,
                attr_password = 0x0007,
                attr_message_integrity = 0x0008,
                attr_error_code = 0x0009,
                attr_unknown_attribute = 0x000a,
                attr_reflected_from = 0x000b,
                attr_realm = 0x0014,
                attr_nonce = 0x0015,
                attr_xor_mapped_address = 0x0020,
            };

            enum class StunState {
                start,
                test_1_started,
                test_1_respond,
                test_2_started,
                test_2_respond,
                test_1_again,
                test_1_again_started,
                test_1_again_respond,
                test_3_started,
                finish,
            };

            enum class StunResult {
                do_request,
                do_roundtrip,
                done,
                encoding_failed,
                respond_error_failed,
                invalid_state,
                unknown_msgtype_failed,
                truncated_failed,
                disconnected_failed,
            };

            bool read_attr(ByteLen& b, Attribute& attr) {
                if (!attr.unpack(b)) {
                    return false;
                }
                return true;
            }

            struct StunContext {
                NATType nat = NATType::unknown;
                StunState state = StunState::start;
                MappedAddress original_address;
                MappedAddress first_time;
                MappedAddress second_time;
                byte transaction_id[12];
                int errcode = 0;

                StunResult request(WPacket& w) {
                    StunHeader h;
                    h.type = msg_binding_request;
                    ByteLen{transaction_id, 12}.copy_to(h.transaction_id, 12);
                    if (state == StunState::start) {
                        nat = NATType::unknown;
                        errcode = 0;
                        first_time = {};
                        second_time = {};
                        h.length = 0;
                        h.pack(w);
                        state = StunState::test_1_started;
                        return StunResult::do_roundtrip;
                    }
                    if (state == StunState::test_1_respond) {
                        h.length = 8;
                        h.pack(w);
                        Attribute attr;
                        attr.type = attr_change_request;
                        attr.length = 4;
                        attr.pack(w);
                        ChangeRequest req;
                        req.change_ip = true;
                        req.change_port = true;
                        req.pack(w);
                        state = StunState::test_2_started;
                        return StunResult::do_roundtrip;
                    }
                    if (state == StunState::test_1_again) {
                        h.length = 0;
                        h.pack(w);
                        state = StunState::test_1_again_started;
                        return StunResult::do_roundtrip;
                    }
                    if (state == StunState::test_1_again_respond) {
                        h.length = 8;
                        h.pack(w);
                        Attribute attr;
                        attr.type = attr_change_request;
                        attr.length = 4;
                        attr.pack(w);
                        ChangeRequest req;
                        req.change_ip = false;
                        req.change_port = true;
                        req.pack(w);
                        state = StunState::test_3_started;
                        return StunResult::do_roundtrip;
                    }
                    return StunResult::invalid_state;
                }

               private:
                StunResult handle_response(ByteLen& b, MappedAddress& mapped) {
                    auto root = b;
                    StunHeader h;
                    if (!h.unpack(b)) {
                        return StunResult::encoding_failed;
                    }
                    if (!b.enough(h.length)) {
                        return StunResult::truncated_failed;
                    }
                    if (h.type == msg_binding_error_response) {
                        Attribute attr;
                        while (read_attr(b, attr)) {
                            if (attr.type == attr_error_code) {
                                ErrorCode code;
                                if (code.unpack(attr.value)) {
                                    errcode = code.code;
                                }
                                break;
                            }
                        }
                        return StunResult::respond_error_failed;
                    }
                    if (h.type == msg_binding_response) {
                        Attribute attr;
                        while (read_attr(b, attr)) {
                            if (attr.type == attr_mapped_address) {
                                if (!mapped.unpack(attr.value)) {
                                    return StunResult::encoding_failed;
                                }
                                break;
                            }
                            if (attr.type == attr_xor_mapped_address) {
                                auto xor_ = root.data + 4;
                                if (!mapped.unpack(attr.value, xor_)) {
                                    return StunResult::encoding_failed;
                                }
                                break;
                            }
                        }
                        return StunResult::do_request;
                    }
                    return StunResult::unknown_msgtype_failed;
                }

               public:
                StunResult no_response() {
                    if (state == StunState::test_1_started) {
                        nat = NATType::udp_blocked;
                    }
                    else if (state == StunState::test_2_started) {
                        if (original_address == first_time) {
                            nat = NATType::udp_firewall;
                        }
                        else {
                            state = StunState::test_1_again;
                            return StunResult::do_request;
                        }
                    }
                    else if (state == StunState::test_1_again_started) {
                        state = StunState::finish;
                        return StunResult::disconnected_failed;
                    }
                    else if (state == StunState::test_3_started) {
                        nat = NATType::port_restricted_cone;
                        state = StunState::finish;
                        return StunResult::done;
                    }
                    else {
                        return StunResult::invalid_state;
                    }
                    state = StunState::finish;
                    return StunResult::done;
                }

                StunResult response(ByteLen& b) {
                    if (state == StunState::test_1_started) {
                        auto res = handle_response(b, first_time);
                        if (res != StunResult::do_request) {
                            state = StunState::finish;
                            return res;
                        }
                        state = StunState::test_1_respond;
                        return res;
                    }
                    if (state == StunState::test_2_started) {
                        MappedAddress mp;
                        auto res = handle_response(b, mp);
                        if (res != StunResult::do_request) {
                            state = StunState::finish;
                            return res;
                        }
                        if (original_address == first_time) {
                            nat = NATType::open_internet;
                        }
                        else {
                            nat = NATType::full_cone;
                        }
                        state = StunState::finish;
                        return StunResult::done;
                    }
                    if (state == StunState::test_1_again_started) {
                        auto res = handle_response(b, second_time);
                        if (res != StunResult::do_request) {
                            return res;
                        }
                        if (first_time != second_time) {
                            nat = NATType::symmetric;
                            state = StunState::finish;
                            return StunResult::done;
                        }
                        state = StunState::test_1_again_respond;
                        return res;
                    }
                    if (state == StunState::test_3_started) {
                        MappedAddress mp;
                        auto res = handle_response(b, mp);
                        if (res != StunResult::do_request) {
                            return res;
                        }
                        nat = NATType::restricted_cone;
                        state = StunState::finish;
                        return StunResult::done;
                    }
                    return StunResult::invalid_state;
                }
            };

        }  // namespace stun
    }      // namespace dnet
}  // namespace utils
