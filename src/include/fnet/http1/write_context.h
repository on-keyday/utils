/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <binary/flags.h>
#include <core/sequencer.h>
#include <wrap/light/enum.h>
#include <number/parse.h>
#include "error_enum.h"

namespace futils::fnet::http1 {

    enum class WriteFlag : std::uint16_t {
        none = 0,
        rough_method = 0x1,
        rough_path = 0x2,
        trust_version = 0x4,
        rough_header_key = 0x8,
        rough_header_value = 0x10,
        legacy_http_0_9 = 0x20,
        trust_phrase = 0x40,

        allow_invalid_content_length = 0x80,                              // this is for experimental use. not recommended
        allow_unexpected_content_length_or_chunked_with_no_body = 0x100,  // this is for experimental use. not recommended
        allow_both_chunked_and_content_length = 0x200,                    // this is for experimental use. not recommended

        allow_no_length_info_body = 0x400,           // this is for experimental use. not recommended
        allow_no_length_even_if_keep_alive = 0x800,  // this is for experimental use. not recommended

        delete_method_has_body = 0x1000,  // this is for experimental use. not recommended

        allow_no_host = 0x2000,  // this is for experimental use. not recommended
    };

    DEFINE_ENUM_FLAGOP(WriteFlag);

    struct WriteContext {
       private:
        size_t content_length_ = 0;
        WriteFlag write_flag_ = WriteFlag::none;
        WriteState state_ = WriteState::uninit;
        futils::binary::flags_t<std::uint32_t, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 1> flag_;
        bits_flag_alias_method(flag_, 0, has_chunked_);
        bits_flag_alias_method(flag_, 1, has_content_length_);
        bits_flag_alias_method(flag_, 2, has_trailer_);
        bits_flag_alias_method(flag_, 3, require_no_body_);
        bits_flag_alias_method(flag_, 4, invalid_content_length_);
        bits_flag_alias_method(flag_, 6, has_close_);
        bits_flag_alias_method(flag_, 7, has_keep_alive_);
        bits_flag_alias_method(flag_, 8, http_major_version_);
        bits_flag_alias_method(flag_, 9, http_minor_version_);
        bits_flag_alias_method(flag_, 10, has_host_);

       public:
        bits_flag_alias_method(flag_, 5, is_server);

        constexpr bool require_host() const noexcept {
            return !is_server() && http_major_version() == 1 && http_minor_version() == 1;
        }

        // reset except WriteFlag
        constexpr void reset() {
            state_ = WriteState::uninit;
            flag_ = 0;
            content_length_ = 0;
        }

        constexpr WriteState state() const noexcept {
            return state_;
        }

        constexpr void set_state(WriteState s) noexcept {
            state_ = s;
        }

        constexpr WriteFlag write_flag() const noexcept {
            return write_flag_;
        }

        constexpr void set_write_flag(WriteFlag f) noexcept {
            write_flag_ = f;
        }

        constexpr bool is_flag(WriteFlag f) {
            return any(write_flag_ & f);
        }

        constexpr void set_flag(WriteFlag f) {
            write_flag_ = f;
        }

        constexpr void add_flag(WriteFlag f) {
            write_flag_ |= f;
        }

        constexpr bool has_chunked() const noexcept {
            return has_chunked_();
        }

        constexpr bool has_content_length() const noexcept {
            return has_content_length_();
        }

        constexpr bool is_invalid_content_length() const noexcept {
            return invalid_content_length_();
        }

        constexpr bool has_trailer() const noexcept {
            return has_trailer_();
        }

        constexpr bool has_host() const noexcept {
            return has_host_();
        }

        constexpr size_t remain_content_length() const noexcept {
            return content_length_;
        }

        constexpr void save_remain_content_length(size_t len) noexcept {
            content_length_ = len;
        }

        constexpr bool no_body() const noexcept {
            return require_no_body_();
        }

        constexpr void scan_method(auto&& method) {
            auto seq = make_ref_seq(method);
            if ((seq.seek_if("GET") || seq.seek_if("HEAD") ||
                 seq.seek_if("OPTIONS") || seq.seek_if("TRACE")) &&
                seq.eos()) {
                require_no_body_(true);
            }
            if (!is_flag(WriteFlag::delete_method_has_body)) {
                seq.rptr = 0;
                if (seq.seek_if("DELETE") && seq.eos()) {
                    require_no_body_(true);
                }
            }
        }

        constexpr void scan_status_code(std::uint16_t code) {
            if (code == 204 || code == 304 || (code >= 100 && code < 200)) {
                require_no_body_(true);
            }
        }

        constexpr void scan_http_version(std::uint8_t major, std::uint8_t minor) {
            http_major_version_(major);
            http_minor_version_(minor);
        }

        constexpr std::uint8_t http_major_version() const noexcept {
            return http_major_version_();
        }

        constexpr std::uint8_t http_minor_version() const noexcept {
            return http_minor_version_();
        }

        constexpr bool has_keep_alive() const noexcept {
            return has_keep_alive_();
        }

        constexpr bool has_close() const noexcept {
            return has_close_();
        }

        // https://www.rfc-editor.org/rfc/rfc9112#name-persistence
        constexpr bool is_keep_alive() const noexcept {
            // this is used for parsing
            return http1::is_keep_alive(true, has_close(), has_keep_alive(), http_major_version(), http_minor_version());
        }

        constexpr void scan_header(auto&& key, auto&& value) {
            auto seq = make_ref_seq(key);
            if (seq.seek_if("Transfer-Encoding", strutil::ignore_case()) && seq.eos()) {
                auto seq2 = make_ref_seq(value);
                while (!seq2.eos()) {
                    if (seq2.seek_if("chunked", strutil::ignore_case())) {
                        has_chunked_(true);
                        break;
                    }
                    seq2.consume();
                }
                return;
            }
            seq.rptr = 0;
            if (seq.seek_if("Content-Length", strutil::ignore_case()) && seq.eos()) {
                if (has_content_length_()) {
                    invalid_content_length_(true);
                }
                has_content_length_(true);
                while (seq.consume_if(' ') || seq.consume_if('\t'));  // skip space
                auto seq2 = make_ref_seq(value);
                if (!number::parse_integer(seq2, content_length_)) {
                    content_length_ = 0;
                    invalid_content_length_(true);
                }
                else {
                    while (seq2.consume_if(' ') || seq2.consume_if('\t'));  // skip space
                    if (!seq2.eos()) {
                        invalid_content_length_(true);
                    }
                }
                return;
            }
            seq.rptr = 0;
            if (seq.seek_if("Trailer", strutil::ignore_case()) && seq.eos()) {
                has_trailer_(true);
                return;
            }
            seq.rptr = 0;
            if (seq.seek_if("Connection", strutil::ignore_case()) && seq.eos()) {
                auto seq2 = make_ref_seq(value);
                while (!seq2.eos()) {
                    if (seq2.seek_if("close", strutil::ignore_case())) {
                        has_close_(true);
                    }
                    if (seq2.seek_if("keep-alive", strutil::ignore_case())) {
                        has_keep_alive_(true);
                    }
                    seq2.consume();
                }
                return;
            }
            seq.rptr = 0;
            if (seq.seek_if("Host", strutil::ignore_case()) && seq.eos()) {
                has_host_(true);
                return;
            }
        }
    };

    constexpr auto sizeof_WriteContext = sizeof(WriteContext);
}  // namespace futils::fnet::http1
