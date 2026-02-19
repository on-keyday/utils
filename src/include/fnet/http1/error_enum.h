/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <wrap/light/enum.h>

namespace futils::fnet::http1 {
    namespace header {
        enum class HeaderError {
            none,
            invalid_header,
            invalid_header_key,
            not_colon,
            invalid_header_value,
            validation_error,
            not_end_of_line,

            invalid_method,
            invalid_path,
            invalid_version,
            invalid_status_code,
            invalid_reason_phrase,
            not_space,

            invalid_state,
            no_data,

            invalid_content_length,
            no_host,
        };

        constexpr const char* to_string(HeaderError e) {
            switch (e) {
                case HeaderError::none:
                    return "none";
                case HeaderError::invalid_header:
                    return "invalid_header";
                case HeaderError::invalid_header_key:
                    return "invalid_header_key";
                case HeaderError::not_colon:
                    return "not_colon";
                case HeaderError::invalid_header_value:
                    return "invalid_header_value";
                case HeaderError::validation_error:
                    return "validation_error";
                case HeaderError::not_end_of_line:
                    return "not_end_of_line";
                case HeaderError::invalid_method:
                    return "invalid_method";
                case HeaderError::invalid_path:
                    return "invalid_path";
                case HeaderError::invalid_version:
                    return "invalid_version";
                case HeaderError::invalid_status_code:
                    return "invalid_status_code";
                case HeaderError::invalid_reason_phrase:
                    return "invalid_reason_phrase";
                case HeaderError::not_space:
                    return "not_space";
                case HeaderError::invalid_state:
                    return "invalid_state";
                case HeaderError::no_data:
                    return "no_data";
                case HeaderError::invalid_content_length:
                    return "invalid_content_length";
                case HeaderError::no_host:
                    return "no_host";
            }
            return "unknown";
        }

        using HeaderErr = wrap::EnumWrap<HeaderError, HeaderError::none, HeaderError::invalid_header>;

    }  // namespace header

    namespace body {
        enum class BodyResult {
            full,         // full of body read
            best_effort,  // best effort read (maybe until disconnect) because no `content-length` or `transfer-encoding: chunked` header
            incomplete,   // incomplete read because of no more data

            invalid_state,    // invalid state
            length_mismatch,  // content-length and actual body length mismatch
            bad_space,        // bad space in chunked data
            bad_line,         // invalid line in chunked data
            invalid_header,   // invalid header (both content-length and chunked)
        };

        constexpr const char* to_string(BodyResult r) {
            switch (r) {
                case BodyResult::full:
                    return "full";
                case BodyResult::best_effort:
                    return "best_effort";
                case BodyResult::incomplete:
                    return "incomplete";
                case BodyResult::invalid_state:
                    return "invalid_state";
                case BodyResult::length_mismatch:
                    return "length_mismatch";
                case BodyResult::bad_space:
                    return "bad_space";
                case BodyResult::bad_line:
                    return "bad_line";
                case BodyResult::invalid_header:
                    return "invalid_header";
            }
            return "unknown";
        }
    }  // namespace body

    enum class WriteState : std::uint8_t {
        uninit,
        header,
        content_length_body,
        chunked_body,
        content_length_chunked_body,  // this is for experimental use. actually malformed
        trailer,
        best_effort_body,
        end,
    };

    constexpr const char* to_string(WriteState s) {
        switch (s) {
            case WriteState::uninit:
                return "uninit";
            case WriteState::header:
                return "header";
            case WriteState::content_length_body:
                return "content_length_body";
            case WriteState::chunked_body:
                return "chunked_body";
            case WriteState::content_length_chunked_body:
                return "content_length_chunked_body";
            case WriteState::trailer:
                return "trailer";
            case WriteState::best_effort_body:
                return "best_effort_body";
            case WriteState::end:
                return "end";
        }
        return "unknown";
    }

    enum class ReadState : std::uint8_t {
        uninit,

        // request line
        method_init,
        method,
        method_space,  // also path_init
        path,
        path_space,  // also request_version_init
        request_version,
        request_version_line_one_byte,
        request_version_line_two_byte,

        // response line
        response_version_init,
        response_version,
        response_version_space,  // also status_code_init
        status_code,
        status_code_space,  // also reason_phrase_init
        reason_phrase,
        reason_phrase_line_one_byte,
        reason_phrase_line_two_byte,

        // header
        header_init,
        header_eol_one_byte,
        header_eol_two_byte,
        header_key,
        header_colon,
        header_pre_space,
        header_value,
        header_last_eol_one_byte,
        header_last_eol_two_byte,

        // body
        body_init,
        body_content_length_init,
        body_content_length,
        body_chunked_init,
        body_chunked_size,
        body_chunked_extension_init,
        body_chunked_extension,
        body_chunked_size_eol_one_byte,
        body_chunked_size_eol_two_byte,
        body_chunked_data_init,
        body_chunked_data,
        body_chunked_data_eol_one_byte,
        body_chunked_data_eol_two_byte,
        body_end,

        trailer_init,
        trailer_eol_one_byte,
        trailer_eol_two_byte,
        trailer_key,
        trailer_colon,
        trailer_pre_space,
        trailer_value,
        trailer_last_eol_one_byte,
        trailer_last_eol_two_byte,
    };

    constexpr const char* to_string(ReadState s) {
        switch (s) {
            case ReadState::uninit:
                return "uninit";
            case ReadState::method_init:
                return "method_init";
            case ReadState::method:
                return "method";
            case ReadState::method_space:
                return "method_space";
            case ReadState::path:
                return "path";
            case ReadState::path_space:
                return "path_space";
            case ReadState::request_version:
                return "request_version";
            case ReadState::request_version_line_one_byte:
                return "request_version_line_one_byte";
            case ReadState::request_version_line_two_byte:
                return "request_version_line_two_byte";
            case ReadState::response_version_init:
                return "response_version_init";
            case ReadState::response_version:
                return "response_version";
            case ReadState::response_version_space:
                return "response_version_space";
            case ReadState::status_code:
                return "status_code";
            case ReadState::status_code_space:
                return "status_code_space";
            case ReadState::reason_phrase:
                return "reason_phrase";
            case ReadState::reason_phrase_line_one_byte:
                return "reason_phrase_line_one_byte";
            case ReadState::reason_phrase_line_two_byte:
                return "reason_phrase_line_two_byte";
            case ReadState::header_init:
                return "header_init";
            case ReadState::header_eol_one_byte:
                return "header_eol_one_byte";
            case ReadState::header_eol_two_byte:
                return "header_eol_two_byte";
            case ReadState::header_key:
                return "header_key";
            case ReadState::header_colon:
                return "header_colon";
            case ReadState::header_pre_space:
                return "header_pre_space";
            case ReadState::header_value:
                return "header_value";
            case ReadState::header_last_eol_one_byte:
                return "header_last_eol_one_byte";
            case ReadState::header_last_eol_two_byte:
                return "header_last_eol_two_byte";
            case ReadState::body_init:
                return "body_init";
            case ReadState::body_content_length_init:
                return "body_content_length_init";
            case ReadState::body_content_length:
                return "body_content_length";
            case ReadState::body_chunked_init:
                return "body_chunked_init";
            case ReadState::body_chunked_size:
                return "body_chunked_size";
            case ReadState::body_chunked_extension_init:
                return "body_chunked_extension_init";
            case ReadState::body_chunked_extension:
                return "body_chunked_extension";
            case ReadState::body_chunked_size_eol_one_byte:
                return "body_chunked_size_eol_one_byte";
            case ReadState::body_chunked_size_eol_two_byte:
                return "body_chunked_size_eol_two_byte";
            case ReadState::body_chunked_data_init:
                return "body_chunked_data_init";
            case ReadState::body_chunked_data:
                return "body_chunked_data";
            case ReadState::body_chunked_data_eol_one_byte:
                return "body_chunked_data_eol_one_byte";
            case ReadState::body_chunked_data_eol_two_byte:
                return "body_chunked_data_eol_two_byte";
            case ReadState::body_end:
                return "body_end";
            case ReadState::trailer_init:
                return "trailer_init";
            case ReadState::trailer_eol_one_byte:
                return "trailer_eol_one_byte";
            case ReadState::trailer_eol_two_byte:
                return "trailer_eol_two_byte";
            case ReadState::trailer_key:
                return "trailer_key";
            case ReadState::trailer_colon:
                return "trailer_colon";
            case ReadState::trailer_pre_space:
                return "trailer_pre_space";
            case ReadState::trailer_value:
                return "trailer_value";
            case ReadState::trailer_last_eol_one_byte:
                return "trailer_last_eol_one_byte";
            case ReadState::trailer_last_eol_two_byte:
                return "trailer_last_eol_two_byte";
            default:
                return "unknown";
        }
    }

    // https://www.rfc-editor.org/rfc/rfc9112#name-persistence
    constexpr bool is_keep_alive(bool is_end, bool has_close, bool has_keep_alive, std::uint8_t major, std::uint8_t minor) noexcept {
        // if state is not end, it's not end of request/response
        // and so keep-alive is not determined
        if (!is_end) {
            return false;
        }
        if (has_close) {
            return false;
        }
        auto is_1_0 = major == 1 && minor == 0;
        auto is_1_1_or_later = major > 1 || (major == 1 && minor >= 1);
        return is_1_0 ? has_keep_alive : is_1_1_or_later || has_keep_alive;
    }

}  // namespace futils::fnet::http1
