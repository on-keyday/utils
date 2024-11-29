/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <wrap/light/enum.h>
#include <core/sequencer.h>
#include <number/parse.h>
#include <strutil/equal.h>
#include <binary/flags.h>
#include "range.h"
#include "state.h"
#include "error_enum.h"

namespace futils::fnet::http1 {

    enum class ReadFlag : std::uint32_t {
        none = 0x0,
        // allow \n as end of line
        allow_only_n = 0x1,
        // allow \r as end of line (not recommended but some client may send this)
        allow_only_r = 0x2,

        // rough request/response line value
        rough_method = 0x4,
        rough_path = 0x8,
        rough_request_version = 0x10,

        rough_response_version = rough_request_version,
        rough_status_code = rough_method,
        rough_status_code_length = rough_path,

        rough_header_key = 0x20,
        rough_header_value = 0x40,

        allow_obs_text = 0x80,  // allow obs-text in header value

        not_trim_pre_space = 0x100,   // do not trim pre space in header value
        not_trim_post_space = 0x200,  // do not trim post space in header value

        not_scan_body_info = 0x400,  // do not scan body info

        // suspend if single chunk is completed
        // this is useful for chunked body with view::rvec
        suspend_on_chunked = 0x800,

        // check consistency between content-length and chunked
        // (not in RFC and not recommended, but some client may send this)
        consistent_chunked_content_length = 0x1000,
        // treat chunked_content_length as chunked (by default, it's treated as error)
        chunked_content_length_as_chunked = 0x2000,

        not_scan_connection_header = 0x4000,  // do not scan connection header

        legacy_http_0_9 = 0x8000,  // allow HTTP/0.9 (not recommended, for experimental use)

        not_scan_trailer_header = 0x10000,  // do not scan trailer header

        not_strict_trailer = 0x20000,  // Trailer header not required strictly for parsing trailer

        delete_method_has_body = 0x40000,  // delete method has body (for compatibility with some client)

        allow_no_host = 0x80000,  // allow no host header (for compatibility with some client)
    };

    DEFINE_ENUM_FLAGOP(ReadFlag);

    enum class BodyType : std::uint8_t {
        no_info,
        chunked,
        content_length,

        // this state is malformed state, but it's useful for some cases
        // by default, it's treated as `chunked`
        // see also https://www.rfc-editor.org/rfc/rfc9112#section-6.3
        chunked_content_length,
    };

    constexpr bool is_header_key_reserved(ReadState s) {
        return s == ReadState::header_colon || s == ReadState::header_pre_space || s == ReadState::header_value;
    }

    constexpr bool is_start(ReadState s) {
        return s == ReadState::uninit;
    }

    constexpr bool is_first_line(ReadState s) {
        switch (s) {
            // request
            case ReadState::method_init:
            case ReadState::method:
            case ReadState::method_space:
            case ReadState::path:
            case ReadState::path_space:
            case ReadState::request_version:
            case ReadState::request_version_line_one_byte:
            case ReadState::request_version_line_two_byte:

            // response
            case ReadState::response_version_init:
            case ReadState::response_version:
            case ReadState::response_version_space:
            case ReadState::status_code:
            case ReadState::status_code_space:
            case ReadState::reason_phrase:
            case ReadState::reason_phrase_line_one_byte:
            case ReadState::reason_phrase_line_two_byte:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_header_line(ReadState s) {
        switch (s) {
            case ReadState::header_init:
            case ReadState::header_eol_one_byte:
            case ReadState::header_eol_two_byte:
            case ReadState::header_key:
            case ReadState::header_colon:
            case ReadState::header_pre_space:
            case ReadState::header_value:
            case ReadState::header_last_eol_one_byte:
            case ReadState::header_last_eol_two_byte:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_trailer_line(ReadState s) {
        switch (s) {
            case ReadState::trailer_init:
            case ReadState::trailer_eol_one_byte:
            case ReadState::trailer_eol_two_byte:
            case ReadState::trailer_key:
            case ReadState::trailer_colon:
            case ReadState::trailer_pre_space:
            case ReadState::trailer_value:
            case ReadState::trailer_last_eol_one_byte:
            case ReadState::trailer_last_eol_two_byte:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_body_in_progress(ReadState s) {
        switch (s) {
            case ReadState::body_init:
            case ReadState::body_content_length_init:
            case ReadState::body_content_length:
            case ReadState::body_chunked_init:
            case ReadState::body_chunked_size:
            case ReadState::body_chunked_extension_init:
            case ReadState::body_chunked_extension:
            case ReadState::body_chunked_size_eol_one_byte:
            case ReadState::body_chunked_size_eol_two_byte:
            case ReadState::body_chunked_data_init:
            case ReadState::body_chunked_data:
            case ReadState::body_chunked_data_eol_one_byte:
            case ReadState::body_chunked_data_eol_two_byte:
                return true;
            default:
                return false;
        }
    }

    struct ReadContext {
       private:
        size_t start_pos_ = 0;
        size_t suspend_pos_ = 0;
        // reuse space for header key start and remain content length
        size_t header_key_start_or_remain_content_ = 0;
        // reuse space for header key end and remain chunk size
        size_t header_key_end_or_remain_chunk_size_ = 0;
        size_t content_length_ = 0;
        ReadFlag flag = ReadFlag::none;
        binary::flags_t<std::uint16_t, 1, 1, 1, 4, 4, 1, 1, 1, 1> flag_;
        bits_flag_alias_method(flag_, 0, resumable);
        bits_flag_alias_method(flag_, 1, has_keep_alive_);
        bits_flag_alias_method(flag_, 2, has_close_);
        bits_flag_alias_method(flag_, 3, http_major_version_);
        bits_flag_alias_method(flag_, 4, http_minor_version_);
        bits_flag_alias_method(flag_, 5, has_host_);
        bits_flag_alias_method(flag_, 6, require_no_body_);
        bits_flag_alias_method(flag_, 7, has_trailer_);
        bits_flag_alias_method(flag_, 8, scan_request_);
        ReadState state_ = ReadState::uninit;
        BodyType body_type_ = BodyType::no_info;

       public:
        constexpr bool scanning_request() const noexcept {
            return scan_request_();
        }

        // uninit state but positions are not reset
        // use reset() to reset all
        constexpr void uninit() {
            state_ = ReadState::uninit;
            body_type_ = BodyType::no_info;
            flag_ = 0;
        }

        // reset all except ReadFlag
        constexpr void reset() {
            uninit();
            start_pos_ = 0;
            suspend_pos_ = 0;
            header_key_start_or_remain_content_ = 0;
            header_key_end_or_remain_chunk_size_ = 0;
            content_length_ = 0;
        }

        constexpr bool has_host() const noexcept {
            return has_host_();
        }

        constexpr bool require_host() const noexcept {
            return http_major_version() == 1 && http_minor_version() == 1 && scan_request_();
        }

        constexpr bool has_trailer() const noexcept {
            return has_trailer_();
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
            return http1::is_keep_alive(state_ == ReadState::body_end, has_close(), has_keep_alive(), http_major_version(), http_minor_version());
        }

        // return removable offset
        // not modify state
        constexpr size_t adjusted_offset() const noexcept {
            if (is_header_key_reserved(state_)) {
                return header_key_start_or_remain_content_;
            }
            return start_pos_;
        }

        // returns removed offset
        // modify state and user must adjust offset of sequencer buffer
        constexpr size_t adjust_offset_to_start() {
            if (is_header_key_reserved(state_)) {
                header_key_end_or_remain_chunk_size_ = header_key_end_or_remain_chunk_size_ - header_key_start_or_remain_content_;
                start_pos_ = start_pos_ - header_key_start_or_remain_content_;
                suspend_pos_ = suspend_pos_ - header_key_start_or_remain_content_;
                auto delta = header_key_start_or_remain_content_;
                header_key_start_or_remain_content_ = 0;
                return delta;
            }
            auto delta = start_pos_;
            start_pos_ = 0;
            suspend_pos_ = suspend_pos_ - delta;
            return delta;
        }

        constexpr bool is_flag(ReadFlag f) const noexcept {
            return any(flag & f);
        }

        constexpr void set_flag(ReadFlag f) noexcept {
            flag = f;
        }

        constexpr void add_flag(ReadFlag f) noexcept {
            flag |= f;
        }

        constexpr void remove_flag(ReadFlag f) noexcept {
            flag &= ~f;
        }

        constexpr void prepare_read(size_t& pos, ReadState initial_state) noexcept {
            if (state_ == ReadState::uninit) {
                state_ = initial_state;
                start_pos_ = pos;
                suspend_pos_ = pos;
            }
            resumable(false);
            pos = suspend_pos_;
        }

        constexpr void save_header_key(size_t start, size_t end) noexcept {
            header_key_start_or_remain_content_ = start;
            header_key_end_or_remain_chunk_size_ = end;
        }

        constexpr void change_state(ReadState new_state, size_t pos) noexcept {
            state_ = new_state;
            start_pos_ = pos;
            suspend_pos_ = pos;
        }

        constexpr void save_pos(size_t pos) noexcept {
            suspend_pos_ = pos;
            resumable(true);
        }

        constexpr void fail_pos(size_t pos) noexcept {
            suspend_pos_ = pos;
            resumable(false);
        }

        constexpr size_t start_pos() const noexcept {
            return start_pos_;
        }

        constexpr ReadState state() const noexcept {
            return state_;
        }

        // Check whether the request/response is suspended and can be resumed with additional data
        // This function is called when the parsing function encounters an error
        // If this function returns true, the parsing function can be called again
        // with the additional data. If false, the error is considered fatal.
        constexpr bool is_resumable() const noexcept {
            return resumable();
        }

        constexpr size_t suspend_pos() const noexcept {
            return suspend_pos_;
        }

        constexpr size_t header_key_start() const noexcept {
            return header_key_start_or_remain_content_;
        }

        constexpr size_t header_key_end() const noexcept {
            return header_key_end_or_remain_chunk_size_;
        }

        constexpr BodyType body_type() const noexcept {
            return body_type_;
        }

        constexpr size_t content_length() const noexcept {
            return content_length_;
        }

        constexpr void save_remain_content_length(size_t remain) noexcept {
            header_key_start_or_remain_content_ = remain;
        }

        constexpr size_t remain_content_length() const noexcept {
            return header_key_start_or_remain_content_;
        }

        constexpr void save_remain_chunk_size(size_t remain) noexcept {
            header_key_end_or_remain_chunk_size_ = remain;
        }

        constexpr size_t remain_chunk_size() const noexcept {
            return header_key_end_or_remain_chunk_size_;
        }

        constexpr void set_body_info(BodyType type, size_t content_length) noexcept {
            body_type_ = type;
            content_length_ = content_length;
        }

        constexpr void scan_http_version(std::uint8_t major, std::uint8_t minor) noexcept {
            http_major_version_(major);
            http_minor_version_(minor);
        }

        template <class T>
        constexpr void scan_method(Sequencer<T>& seq, Range range) {
            auto save = seq.rptr;
            seq.rptr = range.start;
            if ((seq.seek_if("GET") ||
                 seq.seek_if("HEAD") ||
                 seq.seek_if("TRACE") ||
                 seq.seek_if("OPTIONS")) &&
                seq.rptr == range.end) {
                require_no_body_(true);
            }
            if (!is_flag(ReadFlag::delete_method_has_body)) {
                seq.rptr = range.start;
                if (seq.seek_if("DELETE") && seq.rptr == range.end) {
                    require_no_body_(true);
                }
            }
            seq.rptr = save;
            scan_request_(true);
        }

        constexpr void scan_status_code(std::uint16_t code) {
            if (code >= 100 && code < 200 || code == 204 || code == 304) {
                require_no_body_(true);
            }
            scan_request_(false);
        }

        template <class T>
        constexpr void scan_header(Sequencer<T>& seq, FieldRange range) {
            auto save = seq.rptr;
            seq.rptr = range.key.start;
            if (seq.seek_if("host", strutil::ignore_case()) &&
                seq.rptr == range.key.end) {
                has_host_(true);
                seq.rptr = save;
                return;
            }
            if (!is_flag(ReadFlag::not_scan_trailer_header)) {
                if (seq.seek_if("trailer", strutil::ignore_case()) &&
                    seq.rptr == range.key.end) {
                    has_trailer_(true);
                    seq.rptr = save;
                    return;
                }
            }
            if (!is_flag(ReadFlag::not_scan_connection_header)) {
                if (seq.seek_if("connection", strutil::ignore_case()) &&
                    seq.rptr == range.key.end) {
                    seq.rptr = range.value.start;
                    while (seq.rptr < range.value.end) {
                        if (seq.seek_if("close", strutil::ignore_case())) {
                            has_close_(true);
                        }
                        else if (seq.seek_if("keep-alive", strutil::ignore_case())) {
                            has_keep_alive_(true);
                        }
                        seq.consume();
                    }
                }
            }
            if (is_flag(ReadFlag::not_scan_body_info)) {
                seq.rptr = save;
                return;
            }
            if (body_type_ != BodyType::chunked_content_length &&
                body_type_ != BodyType::content_length &&
                seq.seek_if("content-length", strutil::ignore_case()) &&
                seq.rptr == range.key.end) {
                seq.rptr = range.value.start;
                size_t num = 0;
                if (number::parse_integer(seq, num) == number::NumError::none &&
                    seq.rptr == range.value.end) {
                    body_type_ = body_type_ == BodyType::chunked ? BodyType::chunked_content_length : BodyType::content_length;
                    content_length_ = num;
                }
            }
            else if (body_type_ != BodyType::chunked_content_length &&
                     body_type_ != BodyType::chunked &&
                     seq.seek_if("transfer-encoding", strutil::ignore_case()) &&
                     seq.rptr == range.key.end) {
                seq.rptr = range.value.start;
                while (seq.rptr < range.value.end) {
                    if (seq.seek_if("chunked", strutil::ignore_case())) {
                        body_type_ = body_type_ == BodyType::content_length ? BodyType::chunked_content_length : BodyType::chunked;
                        break;
                    }
                    seq.consume();
                }
            }
            seq.rptr = save;
        }

        constexpr bool is_start() const noexcept {
            return http1::is_start(state_);
        }

        constexpr bool is_first_line() const noexcept {
            return http1::is_first_line(state_);
        }

        constexpr bool is_header_line() const noexcept {
            return http1::is_header_line(state_);
        }

        constexpr bool is_trailer_line() const noexcept {
            return http1::is_trailer_line(state_);
        }

        constexpr bool is_body_in_progress() const noexcept {
            return http1::is_body_in_progress(state_);
        }

        constexpr HTTPState http_state() const noexcept {
            if (is_start()) {
                return HTTPState::init;
            }
            if (is_first_line()) {
                return HTTPState::first_line;
            }
            if (is_header_line()) {
                return HTTPState::header;
            }
            if (is_body_in_progress()) {
                return HTTPState::body;
            }
            if (is_trailer_line()) {
                return HTTPState::trailer;
            }
            return HTTPState::end;
        }

        // return true if no body semantics
        // no body semantics means no more data is available
        constexpr bool on_no_body_semantics() noexcept {
            if (state_ == ReadState::body_init && body_type_ == BodyType::no_info) {
                state_ = ReadState::body_end;
                return true;
            }
            return state_ == ReadState::body_end;
        }

        constexpr bool follows_no_body_semantics() noexcept {
            return require_no_body_() && on_no_body_semantics();
        }
    };

    constexpr bool is_no_body_method(auto&& x) {
        return strutil::equal(x, "GET") || strutil::equal(x, "HEAD") || strutil::equal(x, "OPTIONS") || strutil::equal(x, "TRACE");
    }

    constexpr bool is_no_body_status(auto&& x) {
        return x >= 100 && x < 200 || x == 204 || x == 304;
    }

    constexpr auto sizeof_ReadContext = sizeof(ReadContext);

    template <class T>
    constexpr bool read_eol_with_flag(ReadContext& ctx, Sequencer<T>& seq, ReadState one_byte, ReadState two_byte, ReadState next) {
        auto change_state = [&](ReadState state) {
            ctx.change_state(state, seq.rptr);
        };
        auto save_pos = [&] {
            ctx.save_pos(seq.rptr);
        };
        auto fail_pos = [&] {
            ctx.fail_pos(seq.rptr);
        };
        if (ctx.state() == one_byte) {
            if (seq.eos()) {
                save_pos();
                return false;
            }
            if (ctx.is_flag(ReadFlag::allow_only_n)) {
                if (seq.consume_if('\n')) {
                    change_state(next);
                    return true;
                }
            }
            if (!seq.consume_if('\r')) {
                fail_pos();
                return false;
            }
            change_state(two_byte);
        }
        if (ctx.state() == two_byte) {
            if (seq.eos()) {
                save_pos();
                return false;
            }
            if (!seq.consume_if('\n')) {
                if (ctx.is_flag(ReadFlag::allow_only_r)) {
                    change_state(next);
                    return true;
                }
                fail_pos();
                return false;
            }
            change_state(next);
        }
        return true;
    }

    template <class T, class String>
    concept like_ptr = requires(Sequencer<T>& seq) {
        { seq.buf.buffer + seq.rptr };
        { String(seq.buf.buffer + seq.rptr, seq.buf.buffer + seq.rptr) };
    };

    template <class T, class String>
    concept like_range = requires(Sequencer<T>& seq) {
        { seq.buf.buffer.begin() + seq.rptr };
        { String(seq.buf.buffer.begin() + seq.rptr, seq.buf.buffer.begin() + seq.rptr) };
    };

    template <class T, class String>
    constexpr auto range_to_string(Sequencer<T>& seq, String& str, Range range) {
        if constexpr (std::is_assignable_v<String&, Range>) {
            str = range;
        }
        else if constexpr (like_ptr<T, String>) {
            str = String(seq.buf.buffer + range.start, seq.buf.buffer + range.end);
        }
        else if constexpr (like_range<T, String>) {
            str = String(seq.buf.buffer.begin() + range.start, seq.buf.buffer.begin() + range.end);
        }
        else {
            seq.rptr = range.start;
            strutil::read_n(str, seq, range.end - range.start);
        }
    }

    template <class T, class StringOrFunc>
    constexpr auto range_to_string_or_call(Sequencer<T>& seq, StringOrFunc&& str_or_f, Range range) {
        if constexpr (std::invocable<decltype(str_or_f), decltype(seq), Range>) {
            return str_or_f(seq, range);
        }
        else {
            return range_to_string(seq, str_or_f, range);
        }
    }

    // SP / HTAB
    constexpr bool is_tab_or_space(auto c) {
        return c == ' ' || c == '\t';
    }

    constexpr bool is_line(auto c) {
        return c == '\r' || c == '\n';
    }
}  // namespace futils::fnet::http1
