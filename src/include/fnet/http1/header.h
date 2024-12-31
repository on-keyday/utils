/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// header - parse http1 header
#pragma once

#include <strutil/readutil.h>
#include <strutil/strutil.h>
#include <helper/pushbacker.h>
#include <number/parse.h>
#include <strutil/append.h>
#include <strutil/equal.h>
#include <strutil/eol.h>
#include <wrap/light/enum.h>
#include <number/array.h>
#include <fnet/util/uri_lex.h>
#include "read_context.h"
#include "write_context.h"
#include "callback.h"
#include "error_enum.h"

namespace futils::fnet::http1::header {

    struct StatusCode {
        std::uint16_t code = 0;
        void append(auto& v) {
            for (auto& c : v) {
                push_back(c);
            }
        }
        constexpr void push_back(auto v) {
            number::internal::PushBackParserInt<std::uint16_t> tmp;
            tmp.result = code;
            tmp.push_back(v);
            code = tmp.result;
            if (code >= 1000) {
                code /= 10;
            }
        }

        constexpr operator std::uint16_t() const {
            return code;
        }
    };

    // https://www.rfc-editor.org/rfc/rfc9110.html#section-5.6.2
    constexpr bool is_token_char(auto c) {
        return number::is_in_byte_range(c) &&
               (number::is_alnum(c) ||
                // #$%&'*+-.^_`|~!
                c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
                c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' ||
                c == '^' || c == '_' || c == '`' || c == '|' || c == '~');
    }

    // parse_common parse common fields of both http request and http response and http trailer
    // Argument
    // seq - source of header or trailer
    // result - header result function. it should be like this:
    //          HeaderErr(Sequencer<T>& seq, FieldRange range)
    template <class T, class Header>
    constexpr HeaderErr parse_common(ReadContext& ctx, Sequencer<T>& seq, Header&& result) {
        ctx.prepare_read(seq.rptr, ReadState::header_init);
        auto save_pos = [&] {
            ctx.save_pos(seq.rptr);
        };
        auto fail_pos = [&] {
            ctx.fail_pos(seq.rptr);
        };
        auto change_state = [&](ReadState state) {
            ctx.change_state(state, seq.rptr);
        };
        auto change_header_or_trailer = [&](ReadState is_header, ReadState header, ReadState trailer) {
            if (ctx.state() == is_header) {
                change_state(header);
            }
            else {
                change_state(trailer);
            }
        };
        while (true) {
            switch (ctx.state()) {
                case ReadState::header_init:
                case ReadState::trailer_init: {
                    if (seq.eos()) {
                        save_pos();
                        return HeaderError::invalid_header;
                    }
                    if (is_line(seq.current())) {
                        change_header_or_trailer(ReadState::header_init, ReadState::header_last_eol_one_byte, ReadState::trailer_last_eol_one_byte);
                        break;  // jump to eol state
                    }
                    change_header_or_trailer(ReadState::header_init, ReadState::header_key, ReadState::trailer_key);
                    [[fallthrough]];  // fallthrough
                }
                case ReadState::header_key:
                case ReadState::trailer_key: {
                    while (true) {
                        if (seq.eos()) {
                            save_pos();
                            return HeaderError::invalid_header_key;
                        }
                        if (ctx.is_flag(ReadFlag::rough_header_key)) {
                            if (seq.current() == ':') {
                                break;
                            }
                        }
                        else {
                            if (!is_token_char(seq.current())) {
                                break;
                            }
                        }
                        seq.consume();
                    }
                    if (seq.rptr == ctx.start_pos()) {
                        fail_pos();
                        return HeaderError::invalid_header_key;  // empty key
                    }
                    ctx.save_header_key(ctx.start_pos(), seq.rptr);
                    change_header_or_trailer(ReadState::header_key, ReadState::header_colon, ReadState::trailer_colon);
                    [[fallthrough]];
                }
                case ReadState::header_colon:
                case ReadState::trailer_colon: {
                    if (seq.eos()) {
                        save_pos();
                        return HeaderError::not_colon;
                    }
                    if (!seq.consume_if(':')) {
                        fail_pos();
                        return HeaderError::not_colon;
                    }
                    change_header_or_trailer(ReadState::header_colon, ReadState::header_pre_space, ReadState::trailer_pre_space);
                    [[fallthrough]];
                }
                case ReadState::header_pre_space:
                case ReadState::trailer_pre_space: {
                    if (!ctx.is_flag(ReadFlag::not_trim_pre_space)) {
                        while (true) {
                            if (seq.eos()) {
                                save_pos();
                                return HeaderError::invalid_header_value;
                            }
                            if (!is_tab_or_space(seq.current())) {
                                break;
                            }
                            seq.consume();
                        }
                    }
                    change_header_or_trailer(ReadState::header_pre_space, ReadState::header_value, ReadState::trailer_value);
                    [[fallthrough]];
                }
                case ReadState::header_value:
                case ReadState::trailer_value: {
                    while (true) {
                        if (seq.eos()) {
                            save_pos();
                            return HeaderError::invalid_header_value;
                        }
                        if (ctx.is_flag(ReadFlag::rough_header_value)) {
                            if (is_line(seq.current())) {
                                break;
                            }
                        }
                        else {
                            if (ctx.is_flag(ReadFlag::allow_obs_text)) {
                                if (!number::is_non_space_ascii(seq.current()) && !is_tab_or_space(seq.current()) &&
                                    (seq.current() < 0x80 || seq.current() > 0xFF)) {
                                    break;
                                }
                            }
                            else {
                                if (!number::is_non_space_ascii(seq.current()) && !is_tab_or_space(seq.current())) {
                                    break;
                                }
                            }
                        }
                        seq.consume();
                    }
                    auto save = seq.rptr;
                    if (!ctx.is_flag(ReadFlag::not_trim_post_space)) {
                        if (ctx.start_pos() != seq.rptr) {
                            seq.backto();
                            while (is_tab_or_space(seq.current())) {
                                seq.backto();
                            }
                            seq.consume();
                        }
                    }
                    if (seq.rptr == ctx.start_pos()) {
                        fail_pos();
                        return HeaderError::invalid_header_value;  // empty value
                    }
                    FieldRange range;
                    range.key.start = ctx.header_key_start();
                    range.key.end = ctx.header_key_end();
                    range.value.start = ctx.start_pos();
                    range.value.end = seq.rptr;
                    ctx.scan_header(seq, range);
                    // if interrupted here, next call should be with the same state
                    if (auto err = call_and_convert_to_HeaderError(result, seq, range); !err) {
                        seq.rptr = save;  // restore
                        fail_pos();       // save suspend position
                        return err;
                    }
                    seq.rptr = save;  // restore
                    change_header_or_trailer(ReadState::header_value, ReadState::header_eol_one_byte, ReadState::trailer_eol_one_byte);
                    [[fallthrough]];
                }
                case ReadState::header_eol_one_byte:
                case ReadState::header_eol_two_byte:
                case ReadState::trailer_eol_one_byte:
                case ReadState::trailer_eol_two_byte: {
                    if (ctx.state() == ReadState::header_eol_one_byte || ctx.state() == ReadState::header_eol_two_byte) {
                        if (!read_eol_with_flag(ctx, seq, ReadState::header_eol_one_byte, ReadState::header_eol_two_byte, ReadState::header_init)) {
                            return HeaderError::not_end_of_line;
                        }
                    }
                    else {
                        if (!read_eol_with_flag(ctx, seq, ReadState::trailer_eol_one_byte, ReadState::trailer_eol_two_byte, ReadState::trailer_init)) {
                            return HeaderError::not_end_of_line;
                        }
                    }
                    break;
                }
                case ReadState::header_last_eol_one_byte:
                case ReadState::header_last_eol_two_byte:
                case ReadState::trailer_last_eol_one_byte:
                case ReadState::trailer_last_eol_two_byte: {
                    if (ctx.state() == ReadState::header_last_eol_one_byte || ctx.state() == ReadState::header_last_eol_two_byte) {
                        if (!read_eol_with_flag(ctx, seq, ReadState::header_last_eol_one_byte, ReadState::header_last_eol_two_byte, ReadState::body_init)) {
                            return HeaderError::not_end_of_line;
                        }
                        if (ctx.state() == ReadState::body_init && ctx.require_host() && !ctx.has_host() && !ctx.is_flag(ReadFlag::allow_no_host)) {
                            return HeaderError::no_host;
                        }
                    }
                    else {
                        if (!read_eol_with_flag(ctx, seq, ReadState::trailer_last_eol_one_byte, ReadState::trailer_last_eol_two_byte, ReadState::body_end)) {
                            return HeaderError::not_end_of_line;
                        }
                    }
                    return HeaderError::none;
                }
                default: {
                    return HeaderError::none;  // nothing to do
                }
            }
        }
    }

    // HTTP/x.x
    constexpr auto http_version_len = 8;

    template <class T>
    constexpr bool read_http_version(Sequencer<T>& seq, std::uint8_t& major, std::uint8_t& minor) {
        if (!seq.seek_if("HTTP/")) {
            return false;
        }
        major = seq.current();
        if (!number::is_in_byte_range(major) || !number::is_digit(major)) {
            return false;
        }
        seq.consume();
        if (!seq.consume_if('.')) {
            return false;
        }
        minor = seq.current();
        if (!number::is_in_byte_range(minor) || !number::is_digit(minor)) {
            return false;
        }
        seq.consume();
        major = number::number_transform[major];
        minor = number::number_transform[minor];
        return true;
    }

    template <class T>
    constexpr bool read_http_version(ReadContext& ctx, Sequencer<T>& seq) {
        std::uint8_t major, minor;
        if (!read_http_version(seq, major, minor)) {
            return false;
        }
        ctx.scan_http_version(major, minor);
        return true;
    }

    template <class T, class Method, class Path, class Version>
    constexpr HeaderErr parse_request_line(ReadContext& ctx, Sequencer<T>& seq, Method&& method, Path&& path, Version&& version) {
        ctx.prepare_read(seq.rptr, ReadState::method_init);
        auto save_pos = [&] {
            ctx.save_pos(seq.rptr);
        };
        auto change_state = [&](ReadState state) {
            ctx.change_state(state, seq.rptr);
        };
        auto fail_pos = [&] {
            ctx.fail_pos(seq.rptr);
        };
        switch (ctx.state()) {
            case ReadState::method_init: {
                change_state(ReadState::method);
                [[fallthrough]];
            }
            case ReadState::method: {
                while (true) {
                    if (seq.eos()) {
                        save_pos();
                        return HeaderError::invalid_method;
                    }
                    if (ctx.is_flag(ReadFlag::rough_method)) {
                        if (seq.current() == ' ') {
                            break;
                        }
                    }
                    else {
                        if (!is_token_char(seq.current())) {
                            break;
                        }
                    }
                    seq.consume();
                }
                if (seq.rptr == ctx.start_pos()) {
                    fail_pos();
                    return HeaderError::invalid_method;  // empty method
                }
                size_t end = seq.rptr;
                ctx.scan_method(seq, {ctx.start_pos(), end});
                range_to_string_or_call(seq, method, {ctx.start_pos(), end});
                seq.rptr = end;
                change_state(ReadState::method_space);
                [[fallthrough]];
            }
            case ReadState::method_space: {
                if (seq.eos()) {
                    save_pos();
                    return HeaderError::not_space;
                }
                if (!seq.consume_if(' ')) {
                    fail_pos();
                    return HeaderError::not_space;
                }
                change_state(ReadState::path);
                [[fallthrough]];
            }
            case ReadState::path: {
                while (true) {
                    if (seq.eos()) {
                        save_pos();
                        return HeaderError::invalid_path;
                    }
                    if (ctx.is_flag(ReadFlag::rough_path)) {
                        if (seq.current() == ' ') {
                            break;
                        }
                    }
                    else {
                        if (!uri::is_uri_usable(seq.current())) {
                            break;
                        }
                    }
                    seq.consume();
                }
                if (seq.rptr == ctx.start_pos()) {
                    fail_pos();
                    return HeaderError::invalid_path;  // empty path
                }
                size_t end = seq.rptr;
                range_to_string_or_call(seq, path, {ctx.start_pos(), end});
                seq.rptr = end;
                change_state(ReadState::path_space);
                [[fallthrough]];
            }
            case ReadState::path_space: {
                if (seq.eos()) {
                    save_pos();
                    return HeaderError::not_space;
                }
                if (!seq.consume_if(' ')) {
                    // for experimental purpose
                    if (ctx.is_flag(ReadFlag::legacy_http_0_9) && is_line(seq.current())) {
                        ctx.scan_http_version(0, 9);
                        change_state(ReadState::request_version_line_one_byte);
                        break;  // jump to eol state
                    }
                    fail_pos();
                    return HeaderError::not_space;
                }
                change_state(ReadState::request_version);
                [[fallthrough]];
            }
            case ReadState::request_version: {
                if (ctx.is_flag(ReadFlag::rough_request_version)) {
                    while (true) {
                        if (seq.eos()) {
                            save_pos();
                            return HeaderError::invalid_version;
                        }
                        if (is_line(seq.current())) {
                            break;
                        }
                        seq.consume();
                    }
                }
                else {
                    if (seq.remain() < http_version_len) {
                        save_pos();
                        return HeaderError::invalid_version;
                    }
                    if (!read_http_version(ctx, seq)) {  // if here fail, it's fatal error
                        fail_pos();
                        return HeaderError::invalid_version;
                    }
                }
                size_t end = seq.rptr;
                range_to_string_or_call(seq, version, {ctx.start_pos(), end});
                seq.rptr = end;
                change_state(ReadState::request_version_line_one_byte);
                [[fallthrough]];
            }
            case ReadState::request_version_line_one_byte:
            case ReadState::request_version_line_two_byte: {
                if (!read_eol_with_flag(ctx, seq, ReadState::request_version_line_one_byte, ReadState::request_version_line_two_byte, ReadState::header_init)) {
                    return HeaderError::not_end_of_line;
                }
                if (ctx.is_flag(ReadFlag::legacy_http_0_9) && ctx.http_major_version() == 0 && ctx.http_minor_version() == 9) {
                    change_state(ReadState::body_end);  // only GET /path\r\n is allowed for HTTP/0.9 so end here
                    return HeaderError::none;
                }
                break;
            }
            default: {
                break;  // nothing to do
            }
        }
        return HeaderError::none;
    }

    template <class T, class Header, class Method, class Path, class Version>
    constexpr HeaderErr parse_request(ReadContext& ctx, Sequencer<T>& seq, Method&& method, Path&& path, Version&& version, Header&& header) {
        if (auto err = parse_request_line(ctx, seq, method, path, version); !err) {
            return err;
        }
        return parse_common(ctx, seq, header);
    }

    template <class T, class Version, class Status, class Phrase>
    constexpr HeaderErr parse_status_line(ReadContext& ctx, Sequencer<T>& seq, Version&& version, Status&& status, Phrase&& phrase) {
        ctx.prepare_read(seq.rptr, ReadState::response_version_init);
        auto save_pos = [&] {
            ctx.save_pos(seq.rptr);
        };
        auto change_state = [&](ReadState state) {
            ctx.change_state(state, seq.rptr);
        };
        auto fail_pos = [&] {
            ctx.fail_pos(seq.rptr);
        };
        switch (ctx.state()) {
            case ReadState::response_version_init: {
                change_state(ReadState::response_version);
                [[fallthrough]];
            }
            case ReadState::response_version: {
                if (ctx.is_flag(ReadFlag::rough_response_version)) {
                    while (true) {
                        if (seq.eos()) {
                            save_pos();
                            return HeaderError::invalid_version;
                        }
                        if (seq.current() == ' ') {
                            break;
                        }
                        seq.consume();
                    }
                }
                else {
                    if (seq.remain() < http_version_len) {
                        save_pos();
                        return HeaderError::invalid_version;
                    }
                    if (!read_http_version(ctx, seq)) {
                        fail_pos();
                        return HeaderError::invalid_version;
                    }
                }
                size_t end = seq.rptr;
                range_to_string_or_call(seq, version, {ctx.start_pos(), end});
                seq.rptr = end;
                change_state(ReadState::response_version_space);
                [[fallthrough]];
            }
            case ReadState::response_version_space: {
                if (seq.eos()) {
                    save_pos();
                    return HeaderError::not_space;
                }
                if (!seq.consume_if(' ')) {
                    fail_pos();
                    return HeaderError::not_space;
                }
                change_state(ReadState::status_code);
                [[fallthrough]];
            }
            case ReadState::status_code: {
                if (ctx.is_flag(ReadFlag::rough_status_code)) {
                    auto valid_length = [&](bool on_suspend) {
                        if (!ctx.is_flag(ReadFlag::rough_status_code_length)) {
                            if (auto len = (seq.rptr - ctx.start_pos()); len != 3) {
                                // if on_suspend is true and len < 3, it's not error
                                return on_suspend ? len < 3 : false;
                            }
                        }
                        return true;
                    };
                    while (true) {
                        if (seq.eos()) {
                            if (!valid_length(true)) {
                                fail_pos();
                                return HeaderError::invalid_status_code;
                            }
                            save_pos();
                            return HeaderError::invalid_status_code;
                        }
                        if (seq.current() == ' ') {
                            break;
                        }
                        seq.consume();
                    }
                    if (!valid_length(false)) {
                        fail_pos();
                        return HeaderError::invalid_status_code;
                    }
                }
                else {
                    if (seq.remain() < 3) {
                        save_pos();
                        return HeaderError::invalid_status_code;
                    }
                    auto x1 = seq.current();
                    auto x2 = seq.current(1);
                    auto x3 = seq.current(2);
                    if (!number::is_digit(x1)) {
                        fail_pos();
                        return HeaderError::invalid_status_code;
                    }
                    seq.consume();
                    if (!number::is_digit(x2)) {
                        fail_pos();
                        return HeaderError::invalid_status_code;
                    }
                    seq.consume();
                    if (!number::is_digit(x3)) {
                        fail_pos();
                        return HeaderError::invalid_status_code;
                    }
                    seq.consume();
                    ctx.scan_status_code(number::number_transform[x1] * 100 +
                                         number::number_transform[x2] * 10 +
                                         number::number_transform[x3]);
                }
                size_t end = seq.rptr;
                range_to_string_or_call(seq, status, {ctx.start_pos(), end});
                seq.rptr = end;
                change_state(ReadState::status_code_space);
                [[fallthrough]];
            }
            case ReadState::status_code_space: {
                if (seq.eos()) {
                    save_pos();
                    return HeaderError::not_space;
                }
                if (!seq.consume_if(' ')) {
                    fail_pos();
                    return HeaderError::not_space;
                }
                change_state(ReadState::reason_phrase);
                [[fallthrough]];
            }
            case ReadState::reason_phrase: {
                // reason phrase is optional so allow empty
                // see https://www.rfc-editor.org/rfc/rfc9112.html#section-4-4
                while (true) {
                    if (seq.eos()) {
                        save_pos();
                        return HeaderError::invalid_reason_phrase;
                    }
                    if (is_line(seq.current())) {
                        break;
                    }
                    seq.consume();
                }
                size_t end = seq.rptr;
                range_to_string_or_call(seq, phrase, {ctx.start_pos(), end});
                seq.rptr = end;
                change_state(ReadState::reason_phrase_line_one_byte);
                [[fallthrough]];
            }
            case ReadState::reason_phrase_line_one_byte:
            case ReadState::reason_phrase_line_two_byte: {
                if (!read_eol_with_flag(ctx, seq, ReadState::reason_phrase_line_one_byte, ReadState::reason_phrase_line_two_byte, ReadState::header_init)) {
                    return HeaderError::not_end_of_line;
                }
                break;
            }
            default: {
                break;  // nothing to do
            }
        }
        return HeaderError::none;
    }

    template <class T, class Header, class Version, class Status, class Phrase>
    constexpr HeaderErr parse_response(ReadContext& ctx, Sequencer<T>& seq, Version&& version, Status&& status, Phrase&& phrase, Header&& header) {
        if (auto err = parse_status_line(ctx, seq, version, status, phrase); !err) {
            return err;
        }
        return parse_common(ctx, seq, header);
    }

    template <class String>
    constexpr bool is_valid_key(String&& key, bool allow_empty = false) {
        return strutil::validate(key, allow_empty, [](auto&& c) {
            if (!number::is_non_space_ascii(c)) {
                return false;
            }
            if (!is_token_char(c)) {
                return false;
            }
            return true;
        });
    }

    template <class String>
    constexpr bool is_valid_value(String&& value, bool allow_empty = false) {
        return strutil::validate(value, allow_empty, [](auto&& c) {
            return number::is_non_space_ascii(c) || is_tab_or_space(c);
        });
    }

    constexpr auto default_validator(bool key_allow_empty = false, bool value_allow_empty = false) {
        return [=](auto&& key, auto&& value) {
            return is_valid_key(key, key_allow_empty) && is_valid_value(value, value_allow_empty);
        };
    }

    constexpr bool is_http2_pseudo_key(auto&& key) {
        return strutil::equal(key, ":path") ||
               strutil::equal(key, ":authority") ||
               strutil::equal(key, ":status") ||
               strutil::equal(key, ":scheme") ||
               strutil::equal(key, ":method");
    }

    constexpr auto http2_validator(bool key_allow_empty = false, bool value_allow_empty = false) {
        return [=](auto&& key, auto&& value) {
            return (is_http2_pseudo_key(key) || is_valid_key(key, key_allow_empty)) && is_valid_value(value, value_allow_empty);
        };
    }

    template <class Output, class Header, class Validate = decltype(strutil::no_check2())>
    constexpr HeaderErr render_header_common(WriteContext& ctx, Output&& str, Header&& header, Validate&& validate = strutil::no_check2(), bool ignore_invalid = false) {
        if (ctx.state() != WriteState::header &&
            ctx.state() != WriteState::trailer) {
            return HeaderError::invalid_state;
        }
        HeaderError validation_error = HeaderError::none;
        auto header_add = [&](auto&& key, auto&& value) -> HeaderErr {
            if (validation_error != HeaderError::none) {
                return validation_error;
            }
            if (!validate(key, value)) {
                if (!ignore_invalid) {
                    validation_error = HeaderError::validation_error;
                }
                return HeaderError::validation_error;
            }
            ctx.scan_header(key, value);
            strutil::append(str, key);
            strutil::append(str, ": ");
            strutil::append(str, value);
            strutil::append(str, "\r\n");
            return HeaderError::none;
        };
        if (auto err = apply_call_or_iter(header, header_add); !err) {
            return err;
        }
        if (validation_error != HeaderError::none) {
            return validation_error;
        }
        strutil::append(str, "\r\n");
        if (ctx.require_host() && !ctx.has_host() && !ctx.is_flag(WriteFlag::allow_no_host)) {
            return HeaderError::no_host;
        }
        if (ctx.is_invalid_content_length() && !ctx.is_flag(WriteFlag::allow_invalid_content_length)) {
            return HeaderError::invalid_content_length;
        }
        if (ctx.state() == WriteState::trailer) {
            ctx.set_state(WriteState::end);
        }
        else if (ctx.no_body()) {
            if (ctx.has_chunked()) {
                if (!ctx.is_flag(WriteFlag::allow_unexpected_content_length_or_chunked_with_no_body)) {
                    return HeaderError::invalid_content_length;
                }
            }
            if (ctx.has_content_length() && ctx.remain_content_length() != 0) {
                if (!ctx.is_flag(WriteFlag::allow_unexpected_content_length_or_chunked_with_no_body)) {
                    return HeaderError::invalid_content_length;
                }
            }
            ctx.set_state(WriteState::end);
        }
        else if (ctx.has_chunked()) {
            if (ctx.has_content_length()) {
                if (!ctx.is_flag(WriteFlag::allow_both_chunked_and_content_length)) {
                    return HeaderError::invalid_content_length;
                }
                ctx.set_state(WriteState::content_length_chunked_body);
            }
            else {
                ctx.set_state(WriteState::chunked_body);
            }
        }
        else if (ctx.has_content_length()) {
            if (ctx.remain_content_length() == 0) {
                ctx.set_state(WriteState::end);
            }
            else {
                ctx.set_state(WriteState::content_length_body);
            }
        }
        else {
            if (!ctx.is_flag(WriteFlag::allow_no_length_info_body)) {
                return HeaderError::invalid_content_length;
            }
            if (ctx.is_keep_alive()) {
                if (!ctx.is_flag(WriteFlag::allow_no_length_even_if_keep_alive)) {
                    return HeaderError::invalid_content_length;
                }
            }
            ctx.set_state(WriteState::best_effort_body);
        }
        return HeaderError::none;
    }

    template <class Output, class Method, class Path>
    constexpr HeaderErr render_request_line(WriteContext& ctx, Output& str, Method&& method, Path&& path, const char* version_str = "HTTP/1.1") {
        if (ctx.state() != WriteState::uninit) {
            return HeaderError::invalid_state;
        }
        ctx.is_server(false);
        if (ctx.is_flag(WriteFlag::rough_method)) {
            if (strutil::contains(method, " ") ||
                strutil::contains(method, "\r") ||
                strutil::contains(method, "\n")) {
                return HeaderError::invalid_method;
            }
        }
        else {
            if (!is_valid_key(method)) {
                return HeaderError::invalid_method;
            }
        }
        if (ctx.is_flag(WriteFlag::rough_path)) {
            if (strutil::contains(path, " ") ||
                strutil::contains(path, "\r") ||
                strutil::contains(path, "\n")) {
                return HeaderError::invalid_path;
            }
        }
        else {
            if (!strutil::validate(path, false, [](auto&& c) {
                    return uri::is_uri_usable(c);
                })) {
                return HeaderError::invalid_path;
            }
        }
        if (!ctx.is_flag(WriteFlag::legacy_http_0_9) && !version_str) {
            return HeaderError::invalid_version;
        }
        else if (version_str && !ctx.is_flag(WriteFlag::trust_version)) {
            auto seq = make_ref_seq(version_str);
            std::uint8_t major, minor;
            if (!read_http_version(seq, major, minor) || !seq.eos()) {
                return HeaderError::invalid_version;
            }
            ctx.scan_http_version(major, minor);
        }
        ctx.scan_method(method);
        // method
        // ex. GET
        strutil::append(str, method);
        str.push_back(' ');

        // path
        // ex. /index.html
        strutil::append(str, path);

        // version string
        // HTTP/x.x
        if (version_str) {  // if version_str is nullptr, that means this is HTTP/0.9
            str.push_back(' ');
            strutil::append(str, version_str);
        }
        strutil::append(str, "\r\n");

        ctx.set_state(WriteState::header);
        return HeaderError::none;
    }

    template <class String, class Status, class Phrase>
    constexpr HeaderErr render_status_line(WriteContext& ctx, String& str, Status&& status, Phrase&& phrase, const char* version_str = "HTTP/1.1") {
        if (ctx.state() != WriteState::uninit) {
            return HeaderError::invalid_state;
        }
        ctx.is_server(true);
        auto status_ = int(status);
        if (status_ < 100 || status_ > 599) {
            return HeaderError::invalid_status_code;
        }
        ctx.scan_status_code(status_);
        if (!ctx.is_flag(WriteFlag::trust_version)) {
            if (!version_str) {
                return HeaderError::invalid_version;
            }
            auto seq = make_ref_seq(version_str);
            std::uint8_t major, minor;
            if (!read_http_version(seq, major, minor) || !seq.eos()) {
                return HeaderError::invalid_version;
            }
            ctx.scan_http_version(major, minor);
        }
        if (!ctx.is_flag(WriteFlag::trust_phrase)) {
            if (strutil::contains(phrase, "\r") ||
                strutil::contains(phrase, "\n")) {
                return HeaderError::invalid_reason_phrase;
            }
        }
        // version string
        // HTTP/x.x
        strutil::append(str, version_str);
        str.push_back(' ');

        // status code
        // 3 digit number ex. 200
        char code[] = "000";
        code[0] += (status_ / 100);
        code[1] += (status_ % 100 / 10);
        code[2] += (status_ % 10);
        strutil::append(str, code);
        str.push_back(' ');

        // reason phrase (optional)
        strutil::append(str, phrase);
        strutil::append(str, "\r\n");

        ctx.set_state(WriteState::header);
        return HeaderError::none;
    }

    template <class Output, class Method, class Path, class Header, class Validate = decltype(strutil::no_check2())>
    constexpr HeaderErr render_request(WriteContext& ctx, Output& str, Method&& method, Path&& path, Header&& header, Validate&& validate = strutil::no_check2(), bool ignore_invalid = false, const char* version_str = "HTTP/1.1") {
        if (auto err = render_request_line(ctx, str, method, path, version_str); !err) {
            return err;
        }
        return render_header_common(ctx, str, header, validate, ignore_invalid);
    }

    template <class String, class Status, class Phrase, class Header, class Validate = decltype(strutil::no_check2())>
    constexpr HeaderErr render_response(WriteContext& ctx, String& str, Status&& status, Phrase&& phrase, Header&& header,
                                        Validate&& validate = strutil::no_check2(), bool ignore_invalid = false,
                                        const char* version_str = "HTTP/1.1") {
        if (auto err = render_status_line(ctx, str, status, phrase, version_str); !err) {
            return err;
        }
        return render_header_common(ctx, str, header, validate, ignore_invalid);
    }

    void canonical_header(auto&& input, auto&& output) {
        auto seq = make_ref_seq(input);
        bool first = true;
        while (!seq.eos()) {
            if (first) {
                output.push_back(strutil::to_upper(seq.current()));
                first = false;
            }
            else {
                output.push_back(strutil::to_lower(seq.current()));
                if (seq.current() == '-') {
                    first = true;
                }
            }
            seq.consume();
        }
    }

    namespace test {
        using listT = number::Array<std::pair<const char*, const char*>, 10>;

        constexpr auto header_checker(listT& header_list, size_t& index) {
            return [&](auto&& key, auto&& value) {
                if (header_list.size() > index) {
                    if (!strutil::equal(key, header_list[index].first)) {
                        [](auto index) {
                            throw "key not equal";
                        }(index);
                    }
                    if (!strutil::equal(value, header_list[index].second)) {
                        [](auto index) {
                            throw "value not equal";
                        }(index);
                    }
                    index++;
                }
                else {
                    [](auto i, auto list) {
                        throw "header_list size not equal";
                    }(index, header_list);
                }
            };
        }

        constexpr bool test_request(ReadContext& ctx, ReadState expect_state, bool suspend, HeaderError expected_result, const char* expect_method, const char* expect_path, const char* expect_version, listT header_list, const char* text) {
            auto seq = make_ref_seq(text);
            number::Array<char, 20, true> method{}, path{}, version{};
            size_t index = 0;
            auto err = parse_request(
                ctx, seq, method, path, version,
                field_range_to_string<decltype(method)>(header_checker(header_list, index)));
            if (err != expected_result) {
                [](auto err, auto ctx) {
                    throw err;
                }(to_string(err), ctx);
            }
            if (ctx.state() != expect_state) {
                [](auto state, auto ctx) {
                    throw state;
                }(expect_state, ctx);
            }
            if (ctx.is_resumable() != suspend) {
                [](auto suspend, auto ctx) {
                    throw suspend;
                }(suspend, ctx);
            }
            if (err != HeaderError::none) {
                return true;
            }
            if (!strutil::equal(method, expect_method)) {
                throw "method not equal";
            }
            if (!strutil::equal(path, expect_path)) {
                throw "path not equal";
            }
            if (!strutil::equal(version, expect_version)) {
                throw "version not equal";
            }
            return true;
        }

        constexpr bool test_response(ReadContext& ctx, ReadState expect_state, bool suspend, HeaderError expected_result, const char* expect_version, size_t expect_status, const char* expect_phrase, listT header_list, const char* text) {
            auto seq = make_ref_seq(text);
            number::Array<char, 20, true> version{}, phrase{};
            StatusCode status;
            size_t index = 0;
            auto err = parse_response(
                ctx, seq, version, status, phrase,
                field_range_to_string<decltype(version)>([&](auto&& key, auto&& value) {
                    if (header_list.size() > index) {
                        if (!strutil::equal(key, header_list[index].first)) {
                            [](auto index, auto actual, auto expect) {
                                throw "key not equal";
                            }(index, key, header_list[index].first);
                        }
                        if (!strutil::equal(value, header_list[index].second)) {
                            [](auto index, auto actual, auto expect) {
                                throw "value not equal";
                            }(index, value, header_list[index].second);
                        }
                        index++;
                    }
                    else {
                        [](auto i, auto list) {
                            throw "header_list size not equal";
                        }(index, header_list);
                    }
                }));
            if (err != expected_result) {
                [](auto err, auto ctx) {
                    throw err;
                }(to_string(err), ctx);
            }
            if (ctx.state() != expect_state) {
                [](auto state, auto ctx) {
                    throw state;
                }(expect_state, ctx);
            }
            if (ctx.is_resumable() != suspend) {
                [](auto suspend, auto ctx) {
                    throw suspend;
                }(suspend, ctx);
            }
            if (err != HeaderError::none) {
                return true;
            }
            if (!strutil::equal(version, expect_version)) {
                throw "version not equal";
            }
            if (status != expect_status) {
                throw "status not equal";
            }
            if (!strutil::equal(phrase, expect_phrase)) {
                throw "phrase not equal";
            }
            return true;
        }

        constexpr bool test_http_parse() {
            auto test_request = [](HeaderError expected_result, ReadState expect_state, bool suspended, const char* expect_method, const char* expect_path, const char* expect_version, listT header_list, const char* text, bool no_host = true) {
                ReadContext ctx;
                if (no_host) {
                    ctx.set_flag(ReadFlag::allow_no_host);
                }
                test::test_request(ctx, expect_state, suspended, expected_result, expect_method, expect_path, expect_version, header_list, text);
            };
            auto test_response = [](HeaderError expected_result, ReadState expect_state, bool suspended, const char* expect_version, size_t expect_status, const char* expect_phrase, listT header_list, const char* text, bool no_host = true) {
                ReadContext ctx;
                if (no_host) {
                    ctx.set_flag(ReadFlag::allow_no_host);
                }
                test::test_response(ctx, expect_state, suspended, expected_result, expect_version, expect_status, expect_phrase, header_list, text);
            };
            // test request
            // success pattern of request
            test_request(HeaderError::none, ReadState::body_init, false, "GET", "/", "HTTP/1.1", listT{}, "GET / HTTP/1.1\r\n\r\n");
            test_request(HeaderError::none, ReadState::body_init, false, "GET", "/", "HTTP/1.1", listT{{{"key", "value"}}, 1}, "GET / HTTP/1.1\r\nkey: value\r\n\r\n");
            test_request(HeaderError::none, ReadState::body_init, false, "GET", "/", "HTTP/1.1", listT{{{"key", "value"}, {"key2", "value2"}}, 2}, "GET / HTTP/1.1\r\nkey: value\r\nkey2: value2\r\n\r\n");
            test_request(HeaderError::none, ReadState::body_init, false, "GET", "/", "HTTP/1.2", listT{}, "GET / HTTP/1.2\r\n\r\n");
            test_request(HeaderError::none, ReadState::body_init, false, "GET", "/", "HTTP/1.1", listT{{{"Host", "example.com"}}, 1}, "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
            // fail pattern of request
            test_request(HeaderError::invalid_method, ReadState::method, false, "", "/", "HTTP/1.1", listT{}, " / HTTP/1.1\r\n\r\n");
            test_request(HeaderError::invalid_path, ReadState::path, false, "GET", " ", "HTTP/1.1", listT{}, "GET  HTTP/1.1\r\n\r\n");
            test_request(HeaderError::invalid_version, ReadState::request_version, false, "GET", "/", "HTTP/1.2", listT{}, "GET / HTTP/hey\r\n\r\n");
            test_request(HeaderError::not_colon, ReadState::header_colon, false, "GET", "/", "HTTP/1.1", listT{}, "GET / HTTP/1.1\r\nkey\r\n\r\n");
            test_request(HeaderError::invalid_header_key, ReadState::header_key, false, "GET", "/", "HTTP/1.1", listT{}, "GET / HTTP/1.1\r\n: key\r\n\r\n");
            test_request(HeaderError::not_colon, ReadState::header_colon, false, "GET", "/", "HTTP/1.1", listT{{{"key", "value"}}, 1}, "GET / HTTP/1.1\r\nkey: value\r\nkey2\r\n\r\n");
            test_request(HeaderError::invalid_header_value, ReadState::header_value, true, "GET", "/", "HTTP/1.1", listT{}, "GET / HTTP/1.1\r\nkey: value");
            test_request(HeaderError::no_host, ReadState::body_init, false, "GET", "/", "HTTP/1.1", listT{}, "GET / HTTP/1.1\r\n\r\n", false);

            // test response
            // success pattern of response
            test_response(HeaderError::none, ReadState::body_init, false, "HTTP/1.1", 200, "OK", listT{}, "HTTP/1.1 200 OK\r\n\r\n");
            test_response(HeaderError::none, ReadState::body_init, false, "HTTP/1.1", 200, "OK", listT{{{"key", "value"}}, 1}, "HTTP/1.1 200 OK\r\nkey: value\r\n\r\n");
            test_response(HeaderError::none, ReadState::body_init, false, "HTTP/1.1", 200, "OK", listT{{{"key", "value"}, {"key2", "value2"}}, 2}, "HTTP/1.1 200 OK\r\nkey: value\r\nkey2: value2\r\n\r\n");
            test_response(HeaderError::none, ReadState::body_init, false, "HTTP/1.1", 200, "OK", listT{{{"key", "value"}, {"key2", "value2"}}, 2}, "HTTP/1.1 200 OK\r\nkey: value \r\nkey2: value2 \r\n\r\n");
            // fail pattern of response
            test_response(HeaderError::invalid_status_code, ReadState::status_code, false, "HTTP/1.1", 0, "OK", listT{}, "HTTP/1.1 0 OK\r\n\r\n");
            test_response(HeaderError::none, ReadState::body_init, false, "HTTP/1.1", 200, "", listT{}, "HTTP/1.1 200 \r\n\r\n");
            test_response(HeaderError::not_colon, ReadState::header_colon, false, "HTTP/1.1", 200, "OK", listT{}, "HTTP/1.1 200 OK\r\nkey\r\n\r\n");
            test_response(HeaderError::not_colon, ReadState::header_colon, false, "HTTP/1.1", 200, "OK", listT{{{"key", "value"}}, 1}, "HTTP/1.1 200 OK\r\nkey: value\r\nkey2\r\n\r\n");
            test_response(HeaderError::invalid_header_value, ReadState::header_value, true, "HTTP/1.1", 200, "OK", listT{}, "HTTP/1.1 200 OK\r\nkey: value");
            test_response(HeaderError::invalid_header_value, ReadState::header_value, false, "HTTP/1.1", 200, "OK", listT{}, "HTTP/1.1 200 OK\r\nkey: \r\n\r\n");

            return true;
        }

        static_assert(test_http_parse());

        constexpr bool test_suspend() {
            auto suspend_request_test = [](const char* first, size_t delta, const char* next, ReadState expect_state, bool no_host = true) {
                ReadContext ctx;
                if (no_host) {
                    ctx.set_flag(ReadFlag::allow_no_host);
                }
                auto seq = make_ref_seq(first);
                parse_request(ctx, seq, helper::nop, helper::nop, helper::nop, [](auto&&, auto&&) {});
                if (!ctx.is_resumable()) {
                    throw "not suspended";
                }
                auto delta_x = ctx.adjust_offset_to_start();
                if (delta_x != delta) {
                    [](auto delta_x, auto delta) {
                        throw "delta not equal";
                    }(delta_x, delta);
                }
                auto seq2 = make_ref_seq(next);
                parse_request(ctx, seq2, helper::nop, helper::nop, helper::nop, [](auto&&, auto&&) {});
                if (ctx.state() != expect_state) {
                    [](auto state, auto ctx) {
                        throw "state not equal";
                    }(expect_state, ctx);
                }
            };
            auto suspend_response_test = [](const char* first, size_t delta, const char* next, ReadState expect_state) {
                ReadContext ctx;
                auto seq = make_ref_seq(first);
                parse_response(ctx, seq, helper::nop, helper::nop, helper::nop, [](auto&&, auto&&) {});
                if (!ctx.is_resumable()) {
                    throw "not suspended";
                }
                auto delta_x = ctx.adjust_offset_to_start();
                if (delta_x != delta) {
                    [](auto delta_x, auto delta) {
                        throw "delta not equal";
                    }(delta_x, delta);
                }
                auto seq2 = make_ref_seq(next);
                parse_response(ctx, seq2, helper::nop, helper::nop, helper::nop, [](auto&&, auto&&) {});
                if (ctx.state() != expect_state) {
                    [](auto state, auto ctx) {
                        throw "state not equal";
                    }(expect_state, ctx);
                }
            };

            suspend_request_test("GET / HTTP/1.1\r\nkey: value", 16, "key: value\r\n\r\n", ReadState::body_init);
            suspend_request_test("", 0, "G", ReadState::method);
            suspend_request_test("G", 0, "GET", ReadState::method);
            suspend_request_test("GET", 0, "GET /", ReadState::path);
            suspend_request_test("GET / ", 6, "HTTP/1.", ReadState::request_version);
            suspend_request_test("GET / HTTP/1.", 6, "HTTP/1.1", ReadState::request_version_line_one_byte);
            suspend_request_test("GET / HTTP/1.1", 14, "\r", ReadState::request_version_line_two_byte);
            suspend_request_test("GET / HTTP/1.1\r", 15, "\n", ReadState::header_init);
            suspend_request_test("GET / HTTP/1.1\r\n", 16, "key", ReadState::header_key);
            suspend_request_test("GET / HTTP/1.1\r\nkey", 16, "key:", ReadState::header_pre_space);
            suspend_request_test("GET / HTTP/1.1\r\nkey:", 16, "key: ", ReadState::header_pre_space);
            suspend_request_test("GET / HTTP/1.1\r\nkey: ", 16, "key: value", ReadState::header_value);
            suspend_request_test("GET / HTTP/1.1\r\nkey: value", 16, "key: value\r", ReadState::header_eol_two_byte);
            suspend_request_test("GET / HTTP/1.1\r\nkey: value\r", 27, "\n", ReadState::header_init);
            suspend_request_test("GET / HTTP/1.1\r\nkey: value\r\n", 28, "\r", ReadState::header_last_eol_two_byte);

            suspend_response_test("HTTP/1.1 200 OK\r\nkey: value", 17, "key: value\r\n\r\n", ReadState::body_init);
            suspend_response_test("", 0, "H", ReadState::response_version);
            suspend_response_test("H", 0, "HTTP", ReadState::response_version);
            suspend_response_test("HTTP", 0, "HTTP/1.", ReadState::response_version);
            suspend_response_test("HTTP/1.", 0, "HTTP/1.1", ReadState::response_version_space);
            suspend_response_test("HTTP/1.1", 8, " 20", ReadState::status_code);
            suspend_response_test("HTTP/1.1 20", 9, "200", ReadState::status_code_space);
            suspend_response_test("HTTP/1.1 200", 12, " OK", ReadState::reason_phrase);
            suspend_response_test("HTTP/1.1 200 OK", 13, "OK\r", ReadState::reason_phrase_line_two_byte);
            suspend_response_test("HTTP/1.1 200 OK\r", 16, "\n", ReadState::header_init);
            suspend_response_test("HTTP/1.1 200 OK\r\n", 17, "key", ReadState::header_key);
            suspend_response_test("HTTP/1.1 200 OK\r\nkey", 17, "key:", ReadState::header_pre_space);
            suspend_response_test("HTTP/1.1 200 OK\r\nkey:", 17, "key: ", ReadState::header_pre_space);
            suspend_response_test("HTTP/1.1 200 OK\r\nkey: ", 17, "key: value", ReadState::header_value);
            suspend_response_test("HTTP/1.1 200 OK\r\nkey: value", 17, "key: value\r", ReadState::header_eol_two_byte);
            suspend_response_test("HTTP/1.1 200 OK\r\nkey: value\r", 28, "\n", ReadState::header_init);
            suspend_response_test("HTTP/1.1 200 OK\r\nkey: value\r\n", 29, "\r", ReadState::header_last_eol_two_byte);
            return true;
        }

        static_assert(test_suspend());

        constexpr bool render_test() {
            auto report = [](const char* desc, auto... err) {
                throw desc;
            };
            auto request = [&](auto&& method, auto&& path, auto&& header, auto&& expected, WriteState state) {
                WriteContext ctx;
                ctx.set_flag(WriteFlag::allow_no_host);
                futils::number::Array<char, 100> str{};
                if (auto err = render_request(ctx, str, method, path, header); !err) {
                    report("render error", err, ctx, str);
                }
                if (!strutil::equal(str, expected)) {
                    report("not equal", str, futils::strlen(expected));
                }
                if (ctx.state() != state) {
                    report("state not equal", ctx.state(), state);
                }
            };
            auto response = [&](auto&& status, auto&& phrase, auto&& header, auto&& expected, WriteState state) {
                WriteContext ctx;
                futils::number::Array<char, 100> str;
                if (!render_response(ctx, str, status, phrase, header)) {
                    report("render error", ctx, str);
                }
                if (!strutil::equal(str, expected)) {
                    report("not equal", str, futils::strlen(expected));
                }
                if (ctx.state() != state) {
                    report("state not equal", ctx.state(), state);
                }
            };
            request("GET", "/", listT{}, "GET / HTTP/1.1\r\n\r\n", WriteState::end);
            request("GET", "/", listT{{{"key", "value"}}, 1}, "GET / HTTP/1.1\r\nkey: value\r\n\r\n", WriteState::end);
            request("POST", "/index.html", listT{{{"Content-Length", "20"}, {"key", "value"}, {"key2", "value2"}}, 3}, "POST /index.html HTTP/1.1\r\nContent-Length: 20\r\nkey: value\r\nkey2: value2\r\n\r\n", WriteState::content_length_body);
            request("POST", "/index.html", listT{{{"Transfer-Encoding", "  chunked"}, {"key", "value"}, {"key2", "value2"}}, 3}, "POST /index.html HTTP/1.1\r\nTransfer-Encoding:   chunked\r\nkey: value\r\nkey2: value2\r\n\r\n", WriteState::chunked_body);
            response(204, "No Content", listT{}, "HTTP/1.1 204 No Content\r\n\r\n", WriteState::end);
            response(200, "OK", listT{{{"Content-Length", "0"}, {"key", "value"}}, 2}, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nkey: value\r\n\r\n", WriteState::end);
            response(200, "OK", listT{{{"Content-Length", "12"}, {"key", "value"}}, 2}, "HTTP/1.1 200 OK\r\nContent-Length: 12\r\nkey: value\r\n\r\n", WriteState::content_length_body);
            response(200, "OK", listT{{{"Transfer-Encoding", "  chunked"}, {"key", "value"}}, 2}, "HTTP/1.1 200 OK\r\nTransfer-Encoding:   chunked\r\nkey: value\r\n\r\n", WriteState::chunked_body);

            return true;
        }

        static_assert(render_test());
    }  // namespace test

}  // namespace futils::fnet::http1::header
