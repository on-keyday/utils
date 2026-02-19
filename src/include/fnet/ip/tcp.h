/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "ip.h"
#include <wrap/light/enum.h>
#include <variant>
#include <view/span.h>

namespace futils::fnet::tcp {

    enum class TCPState : byte {
        LISTEN,
        SYN_SENT,
        SYN_RECEIVED,
        ESTABLISHED,
        FIN_WAIT_1,
        FIN_WAIT_2,
        CLOSE_WAIT,
        CLOSING,
        LAST_ACK,
        TIME_WAIT,
        CLOSED
    };

    enum class TCPCommand : byte {
        OPEN,
        SEND,
        CLOSE,
        RECEIVE,
    };

    struct StateMachine {
       private:
        TCPState state = TCPState::CLOSED;

       public:
        // send SYN
        constexpr expected<void> open() {
            if (state == TCPState::CLOSED ||
                state == TCPState::LISTEN) {
                state = TCPState::SYN_SENT;
                return {};
            }
            return unexpect("already opened");
        }

        // recv SYN/ACK send ACK
        constexpr expected<void> open_ack() {
            if (state == TCPState::SYN_SENT) {
                state = TCPState::ESTABLISHED;
                return {};
            }
            return unexpect("not in opening");
        }

        constexpr expected<void> open_lost() {
            if (state == TCPState::SYN_SENT) {
                state = TCPState::CLOSED;
                return {};
            }
            return unexpect("not sent SYN");
        }

        constexpr expected<void> listen() {
            if (state == TCPState::CLOSED) {
                state = TCPState::LISTEN;
                return {};
            }
            return unexpect("not in closed");
        }

        constexpr expected<void> accept() {
            if (state == TCPState::LISTEN) {
                state = TCPState::SYN_RECEIVED;
                return {};
            }
            return unexpect("not in listening");
        }

        // recv ACK
        constexpr expected<void> accept_ack() {
            if (state == TCPState::SYN_RECEIVED) {
                state = TCPState::ESTABLISHED;
                return {};
            }
            return unexpect("not received SYN");
        }

        // send FIN
        constexpr expected<void> accept_lost() {
            if (state == TCPState::SYN_RECEIVED) {
                state = TCPState::FIN_WAIT_1;
                return {};
            }
            return unexpect("not received SYN");
        }

        // send FIN
        constexpr expected<void> close() {
            if (state == TCPState::ESTABLISHED) {
                state = TCPState::FIN_WAIT_1;
                return {};
            }
            else if (state == TCPState::CLOSE_WAIT) {
                state = TCPState::LAST_ACK;
                return {};
            }
            return unexpect("connection not ESTABLISHED or not in CLOSE_WAIT");
        }

        // recv ACK of FIN
        constexpr expected<void> close_ack() {
            if (state == TCPState::FIN_WAIT_1) {
                state = TCPState::FIN_WAIT_2;
                return {};
            }
            else if (state == TCPState::CLOSING) {
                state = TCPState::TIME_WAIT;
                return {};
            }
            else if (state == TCPState::LAST_ACK) {
                state = TCPState::CLOSED;
                return {};
            }
            return unexpect("unexpected ack for FIN");
        }

        // recv FIN
        constexpr expected<void> close_remote() {
            if (state == TCPState::ESTABLISHED) {
                state = TCPState::CLOSE_WAIT;
            }
            else if (state == TCPState::FIN_WAIT_1) {
                state = TCPState::CLOSING;
                return {};
            }
            else if (state == TCPState::FIN_WAIT_2) {
                state = TCPState::TIME_WAIT;
                return {};
            }
            return unexpect("unexpected FIN from remote");
        }
    };

    enum class TCPPacketType : byte {
        NUL = 0x0,
        SYN = 0x1,
        ACK = 0x2,
        FIN = 0x4,
        RST = 0x8,
        PUSH = 0x10,
        URG = 0x20,
        ECN = 0x40,
    };

    DEFINE_ENUM_FLAGOP(TCPPacketType);

    struct TCPHeader {
        std::uint16_t src_port = 0;
        std::uint16_t dst_port = 0;
        std::uint32_t seq_number = 0;
        std::uint32_t ack_number = 0;

       private:
        binary::flags_t<std::uint16_t, 4, 1, 3,
                        1, 1, 1, 1, 1, 1, 1, 1>
            flags;
        bits_flag_alias_method(flags, 0, header_length_4bit);

       public:
        constexpr bool set_header_length(byte length) {
            if (length < 20 || length > 60) {
                return false;
            }
            return set_header_length_4bit(length >> 2);
        }

        constexpr byte header_length() const noexcept {
            return header_length_4bit() << 2;
        }

        bits_flag_alias_method(flags, 1, death);  // based on RFC9401, this is joke
        bits_flag_alias_method(flags, 2, reserved);
        bits_flag_alias_method(flags, 3, congestion_window_reduced);
        bits_flag_alias_method(flags, 4, ecn_echo);
        bits_flag_alias_method(flags, 5, urgent);
        bits_flag_alias_method(flags, 6, ack);
        bits_flag_alias_method(flags, 7, push);
        bits_flag_alias_method(flags, 8, reset);
        bits_flag_alias_method(flags, 9, syn);
        bits_flag_alias_method(flags, 10, fin);
        std::uint16_t window_size = 0;
        std::uint16_t check_sum = 0;
        std::uint16_t urgent_pointer = 0;

        constexpr bool parse(binary::reader& r) noexcept {
            return binary::read_num_bulk(r, true, src_port, dst_port, seq_number, ack_number, flags.as_value(), window_size, check_sum, urgent_pointer);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return binary::write_num_bulk(w, true, src_port, dst_port, seq_number, ack_number, flags.as_value(), window_size, check_sum, urgent_pointer);
        }

        constexpr TCPPacketType type() const noexcept {
            TCPPacketType flag = TCPPacketType::NUL;
            if (syn()) {
                flag |= TCPPacketType::SYN;
            }
            if (ack()) {
                flag |= TCPPacketType::ACK;
            }
            if (fin()) {
                flag |= TCPPacketType::FIN;
            }
            if (reset()) {
                flag |= TCPPacketType::RST;
            }
            if (push()) {
                flag |= TCPPacketType::PUSH;
            }
            if (urgent()) {
                flag |= TCPPacketType::URG;
            }
            if (ecn_echo()) {
                flag |= TCPPacketType::ECN;
            }
            return flag;
        }

        constexpr void set_type(TCPPacketType flag) noexcept {
            set_syn(any(flag & TCPPacketType::SYN));
            set_ack(any(flag & TCPPacketType::ACK));
            set_fin(any(flag & TCPPacketType::FIN));
            set_reset(any(flag & TCPPacketType::RST));
            set_push(any(flag & TCPPacketType::PUSH));
            set_urgent(any(flag & TCPPacketType::URG));
            set_ecn_echo(any(flag & TCPPacketType::ECN));
        }
    };

    namespace option {
        enum class TCPOptionID : uint8_t {
            end_of_option_list = 0,
            no_operation = 1,
            maximum_segment_size = 2,
            window_scale = 3,
            sack_permitted = 4,
            sack = 5,
            echo = 6,
            echo_reply = 7,
            timestamps = 8,
            partial_order_connection_permitted = 9,
            partial_order_service_profile = 10,
            cc = 11,
            cc_new = 12,
            cc_echo = 13,
            tcp_alternate_checksum_request = 14,
            tcp_alternate_checksum_data = 15,
            skeeter = 16,
            bubba = 17,
            trailer_checksum_option = 18,
            md5_signature_option = 19,
            scps_capabilities = 20,
            selective_negative_acknowledgements = 21,
            record_boundaries = 22,
            corruption_experienced = 23,
            snap = 24,
            unassigned = 25,
            tcp_compression_filter = 26,
            quick_start_response = 27,
            user_timeout_option = 28,
            tcp_authentication_option = 29,
            multipath_tcp = 30,
            reserved_31 = 31,
            reserved_32 = 32,
            reserved_33 = 33,
            tcp_fast_open_cookie = 34,
            reserved_35_68 = 35,  // Range
            encryption_negotiation = 69,
            reserved_70 = 70,
            reserved_71_75 = 71,  // Range
            reserved_76 = 76,
            reserved_77 = 77,
            reserved_78 = 78,
            reserved_79_171 = 79,  // Range
            accurate_ecn_order_0 = 172,
            reserved_173 = 173,
            accurate_ecn_order_1 = 174,
            reserved_175_252 = 175,  // Range
            rfc3692_experiment_1 = 253,
            rfc3692_experiment_2 = 254
        };

        constexpr const char* to_string(TCPOptionID id) {
            switch (id) {
                case TCPOptionID::end_of_option_list:
                    return "END_OF_OPTION_LIST";
                case TCPOptionID::no_operation:
                    return "NO_OPERATION";
                case TCPOptionID::maximum_segment_size:
                    return "MAXIMUM_SEGMENT_SIZE";
                case TCPOptionID::window_scale:
                    return "WINDOW_SCALE";
                case TCPOptionID::sack_permitted:
                    return "SACK_PERMITTED";
                case TCPOptionID::sack:
                    return "SACK";
                case TCPOptionID::echo:
                    return "ECHO";
                case TCPOptionID::echo_reply:
                    return "ECHO_REPLY";
                case TCPOptionID::timestamps:
                    return "TIMESTAMPS";
                case TCPOptionID::partial_order_connection_permitted:
                    return "PARTIAL_ORDER_CONNECTION_PERMITTED";
                case TCPOptionID::partial_order_service_profile:
                    return "PARTIAL_ORDER_SERVICE_PROFILE";
                case TCPOptionID::cc:
                    return "CC";
                case TCPOptionID::cc_new:
                    return "CC_NEW";
                case TCPOptionID::cc_echo:
                    return "CC_ECHO";
                case TCPOptionID::tcp_alternate_checksum_request:
                    return "TCP_ALTERNATE_CHECKSUM_REQUEST";
                case TCPOptionID::tcp_alternate_checksum_data:
                    return "TCP_ALTERNATE_CHECKSUM_DATA";
                case TCPOptionID::skeeter:
                    return "SKEETER";
                case TCPOptionID::bubba:
                    return "BUBBA";
                case TCPOptionID::trailer_checksum_option:
                    return "TRAILER_CHECKSUM_OPTION";
                case TCPOptionID::md5_signature_option:
                    return "MD5_SIGNATURE_OPTION";
                case TCPOptionID::scps_capabilities:
                    return "SCPS_CAPABILITIES";
                case TCPOptionID::selective_negative_acknowledgements:
                    return "SELECTIVE_NEGATIVE_ACKNOWLEDGEMENTS";
                case TCPOptionID::record_boundaries:
                    return "RECORD_BOUNDARIES";
                case TCPOptionID::corruption_experienced:
                    return "CORRUPTION_EXPERIENCED";
                case TCPOptionID::snap:
                    return "SNAP";
                case TCPOptionID::unassigned:
                    return "UNASSIGNED";
                case TCPOptionID::tcp_compression_filter:
                    return "TCP_COMPRESSION_FILTER";
                case TCPOptionID::quick_start_response:
                    return "QUICK_START_RESPONSE";
                case TCPOptionID::user_timeout_option:
                    return "USER_TIMEOUT_OPTION";
                case TCPOptionID::tcp_authentication_option:
                    return "TCP_AUTHENTICATION_OPTION";
                case TCPOptionID::multipath_tcp:
                    return "MULTIPATH_TCP";
                case TCPOptionID::reserved_31:
                    return "RESERVED_31";
                case TCPOptionID::reserved_32:
                    return "RESERVED_32";
                case TCPOptionID::reserved_33:
                    return "RESERVED_33";
                case TCPOptionID::tcp_fast_open_cookie:
                    return "TCP_FAST_OPEN_COOKIE";
                case TCPOptionID::reserved_35_68:
                    return "RESERVED_35_68";
                case TCPOptionID::encryption_negotiation:
                    return "ENCRYPTION_NEGOTIATION";
                case TCPOptionID::reserved_70:
                    return "RESERVED_70";
                case TCPOptionID::reserved_71_75:
                    return "RESERVED_71_75";
                case TCPOptionID::reserved_76:
                    return "RESERVED_76";
                case TCPOptionID::reserved_77:
                    return "RESERVED_77";
                case TCPOptionID::reserved_78:
                    return "RESERVED_78";
                case TCPOptionID::reserved_79_171:
                    return "RESERVED_79_171";
                case TCPOptionID::accurate_ecn_order_0:
                    return "ACCURATE_ECN_ORDER_0";
                case TCPOptionID::reserved_173:
                    return "RESERVED_173";
                case TCPOptionID::accurate_ecn_order_1:
                    return "ACCURATE_ECN_ORDER_1";
                case TCPOptionID::reserved_175_252:
                    return "RESERVED_175_252";
                case TCPOptionID::rfc3692_experiment_1:
                    return "RFC3692_EXPERIMENT_1";
                case TCPOptionID::rfc3692_experiment_2:
                    return "RFC3692_EXPERIMENT_2";
                default:
                    return "UNKNOWN_OPTION";
            }
        }

        struct UnspecOption {
            view::rvec data;
            TCPOptionID get_id() const noexcept {
                return TCPOptionID::no_operation;  // this is handled as any option
            }

            constexpr bool is_fixed() const noexcept {
                return false;
            }

            constexpr size_t size() const {
                return data.size();
            }

            constexpr bool parse(binary::reader& r, byte len) noexcept {
                return r.read(data, len);
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return w.write(data);
            }
        };

        struct MaxSegmentSize {
            std::uint16_t mss = 0;

            TCPOptionID get_id() const noexcept {
                return TCPOptionID::maximum_segment_size;
            }

            constexpr bool is_fixed() const noexcept {
                return true;
            }

            constexpr size_t size() const {
                return 2;
            }

            constexpr bool parse(binary::reader& r, byte len) {
                return len == 2 && binary::read_num(r, mss);
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return binary::write_num(w, mss);
            }
        };

        struct WindowScale {
            std::uint8_t shift_count = 0;

            TCPOptionID get_id() const noexcept {
                return TCPOptionID::window_scale;
            }

            constexpr bool is_fixed() const noexcept {
                return true;
            }

            constexpr size_t size() const {
                return 1;
            }

            constexpr bool parse(binary::reader& r, std::uint8_t len) {
                return binary::read_num(r, shift_count);
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return binary::write_num(w, shift_count);
            }
        };

        struct Timestamps {
            std::uint32_t ts_value = 0;
            std::uint32_t ts_echo_reply = 0;

            constexpr size_t size() const {
                return 8;
            }

            TCPOptionID get_id() const noexcept {
                return TCPOptionID::timestamps;
            }

            constexpr bool is_fixed() const noexcept {
                return true;
            }

            constexpr bool parse(binary::reader& r, byte len) {
                return binary::read_num(r, ts_value) && binary::read_num(r, ts_echo_reply);
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return binary::write_num(w, ts_value) && binary::write_num(w, ts_echo_reply);
            }
        };

        struct SackPermitted {
            constexpr size_t size() const noexcept {
                return 0;
            }

            constexpr bool is_fixed() const noexcept {
                return true;
            }

            constexpr bool parse(binary::reader& r, byte len) noexcept {
                return true;
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return true;
            }

            TCPOptionID get_id() const noexcept {
                return TCPOptionID::sack_permitted;
            }
        };

        struct Sack {
            // ここに SACK オプションのデータ構造を追加する
            using Range = std::pair<std::uint32_t, std::uint32_t>;
            Range sack_ranges[3];
            byte count = 0;

            constexpr bool is_fixed() const noexcept {
                return false;  // SACK オプションは可変長なので false を返す
            }

            constexpr size_t size() const {
                // SACK オプションのサイズ計算を実装する
                return count * 8;
            }

            constexpr bool parse(binary::reader& r, byte len) noexcept {
                count = len >> 3;
                if (count < 1 || 3 < count) {
                    // NOTE(on-keyday): currently we trust over 3 sack is not used
                    // see https://datatracker.ietf.org/doc/html/rfc2018#section-3
                    return false;
                }
                // SACK オプションのパースを実装する
                auto [data, ok] = r.read(len);
                if (!ok) {
                    return false;
                }
                binary::reader s{data};
                auto ptr = sack_ranges;
                while (!s.empty()) {
                    if (!binary::read_num_bulk(s, true, ptr->first, ptr->second)) {
                        return false;
                    }
                    ptr++;
                }
                return true;
            }

            constexpr bool render(binary::writer& w) const noexcept {
                if (count < 1 || 3 < count) {
                    return false;
                }
                for (auto i = 0; i < count; i++) {
                    if (!binary::write_num_bulk(w, true, sack_ranges[i].first, sack_ranges[i].second)) {
                        return false;
                    }
                }
                return true;
            }

            TCPOptionID get_id() const noexcept {
                return TCPOptionID::sack;
            }
        };

        using Option = std::variant<UnspecOption, MaxSegmentSize, WindowScale, Timestamps, SackPermitted, Sack>;

        constexpr auto sizeof_Option = sizeof(Option);
        struct OptionPayload {
           private:
            Option option;

           public:
            constexpr void set_option(Option&& opt) {
                option = std::move(opt);
            }

            template <class T>
            constexpr T* get_option() {
                return std::get_if<T>(&option);
            }

            // len is only option data size, exclude option kind and option length field
            // len = option length field - 2
            constexpr bool parse(binary::reader& r, TCPOptionID id, byte len) noexcept {
                switch (id) {
                    case TCPOptionID::maximum_segment_size:
                        option = MaxSegmentSize{};
                        break;
                    case TCPOptionID::window_scale:
                        option = WindowScale{};
                        break;
                    case TCPOptionID::timestamps:
                        option = Timestamps{};
                        break;
                    case TCPOptionID::sack_permitted:
                        option = SackPermitted{};
                        break;
                    default:
                        option = UnspecOption{};
                        break;
                }
                if (is_fixed()) {
                    if (len != size()) {
                        return false;
                    }
                }
                return std::visit([&](auto&& obj) {
                    return obj.parse(r, len);
                },
                                  option);
            }

            // len is only option data size, exclude option kind and option length field
            // len = option length field - 2
            constexpr size_t size() const noexcept {
                return std::visit([&](auto&& obj) {
                    return obj.size();
                },
                                  option);
            }

            constexpr TCPOptionID get_id() const noexcept {
                return std::visit([&](auto&& obj) {
                    return obj.get_id();
                },
                                  option);
            }

            constexpr bool is_fixed() const noexcept {
                return std::visit([&](auto&& obj) {
                    return obj.is_fixed();
                },
                                  option);
            }

            constexpr bool render(binary::writer& w, TCPOptionID id) const noexcept {
                auto cur_id = get_id();
                if (cur_id != TCPOptionID::no_operation &&
                    cur_id != id) {
                    return false;
                }
                return std::visit([&](auto&& obj) {
                    return obj.render(w);
                },
                                  option);
            }
        };

        struct TCPOption {
            TCPOptionID id = TCPOptionID::end_of_option_list;
            byte len = 0;
            OptionPayload data;

            constexpr TCPOption() = default;
            constexpr TCPOption(Option&& o) {
                data.set_option(std::move(o));
            }

            constexpr bool parse(binary::reader& r) noexcept {
                if (!binary::read_num(r, id)) {
                    return false;
                }
                if (id == TCPOptionID::end_of_option_list || id == TCPOptionID::no_operation) {
                    return true;
                }
                return binary::read_num(r, len) &&
                       data.parse(r, id, len - 2);
            }

            constexpr bool render(binary::writer& w) const noexcept {
                if (!binary::write_num(w, id)) {
                    return false;
                }
                if (id == TCPOptionID::end_of_option_list || id == TCPOptionID::no_operation) {
                    return true;
                }
                if (data.size() > 0xFD) {
                    return false;
                }
                return binary::write_num(w, std::uint8_t(data.size() + 2)) &&
                       data.render(w, id);
            }

            constexpr void set_option(Option&& opt) {
                data.set_option(std::move(opt));
                id = data.get_id();
            }
        };

        expected<void> parse_tcp_options(TCPHeader& hdr, binary::reader& r, auto&& handle) {
            auto length = hdr.header_length();
            if (length == 20) {
                return {};  // no option available
            }
            auto [data, ok] = r.read(length - 20);
            if (!ok) {
                return unexpect("not enough length");  // not enough length
            }
            binary::reader s{data};
            while (!s.empty()) {
                TCPOption opt;
                if (!opt.parse(s)) {
                    return unexpect("option parse failed");
                }
                if (auto ok = handle(opt); !ok) {
                    return ok;
                }
                if (opt.id == TCPOptionID::end_of_option_list) {
                    break;  // not to see padding
                }
            }
            return {};
        }
    }  // namespace option
}  // namespace futils::fnet::tcp
