/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// body - http body reading
#pragma once

#include <core/sequencer.h>
#include <strutil/readutil.h>
#include <strutil/eol.h>
#include <number/parse.h>
#include <strutil/equal.h>
#include <number/to_string.h>
#include <view/sized.h>
#include "read_context.h"
#include "write_context.h"
#include "error_enum.h"

namespace futils::fnet::http1::body {

    // read_body reads http body with BodyType
    template <class String, class Extension = decltype(helper::nop)&, class T>
    constexpr BodyResult read_body(ReadContext& ctx, Sequencer<T>& seq, String&& body, Extension&& chunked_extension = helper::nop) {
        ctx.prepare_read(seq.rptr, ReadState::body_init);
        auto save_pos = [&] {
            ctx.save_pos(seq.rptr);
        };
        auto change_state = [&](ReadState state) {
            ctx.change_state(state, seq.rptr);
        };
        while (true) {
            switch (ctx.state()) {
                case ReadState::body_init: {
                    switch (ctx.body_type()) {
                        case BodyType::no_info: {
                            // best effort means read until disconnect, so just read all
                            range_to_string_or_call(seq, body, {seq.rptr, seq.rptr + seq.remain()});
                            seq.rptr = seq.size();
                            return BodyResult::best_effort;
                        }
                        case BodyType::content_length: {
                            change_state(ReadState::body_content_length_init);
                            break;
                        }
                        case BodyType::chunked: {
                            change_state(ReadState::body_chunked_init);
                            break;
                        }
                        case BodyType::chunked_content_length: {
                            if (ctx.is_flag(ReadFlag::chunked_content_length_as_chunked) ||
                                ctx.is_flag(ReadFlag::consistent_chunked_content_length)) {
                                // both chunked_content_length_as_chunked and
                                // consistent_chunked_content_length are invalid
                                if (ctx.is_flag(ReadFlag::chunked_content_length_as_chunked) &&
                                    ctx.is_flag(ReadFlag::consistent_chunked_content_length)) {
                                    return BodyResult::invalid_state;
                                }
                                change_state(ReadState::body_chunked_init);
                            }
                            else {
                                return BodyResult::invalid_header;
                            }
                        }
                    }
                    break;  // jump to next state
                }
                case ReadState::body_content_length_init: {
                    if (ctx.content_length() == 0) {
                        change_state(ReadState::body_end);
                        return BodyResult::full;
                    }
                    ctx.save_remain_content_length(ctx.content_length());
                    change_state(ReadState::body_content_length);
                    [[fallthrough]];  // fallthrough
                }
                case ReadState::body_content_length: {
                    if (seq.eos()) {
                        save_pos();
                        return BodyResult::incomplete;
                    }
                    auto remain_content = ctx.remain_content_length();
                    auto remain_input = seq.remain();
                    auto base_pos = seq.rptr;
                    if (remain_input < remain_content) {
                        range_to_string_or_call(seq, body, {seq.rptr, seq.rptr + seq.remain()});
                        seq.rptr = base_pos + remain_input;
                        ctx.save_remain_content_length(remain_content - remain_input);
                        save_pos();
                        return BodyResult::incomplete;
                    }
                    range_to_string_or_call(seq, body, {seq.rptr, seq.rptr + remain_content});
                    seq.rptr = base_pos + remain_content;
                    ctx.save_remain_content_length(0);
                    change_state(ReadState::body_end);
                    return BodyResult::full;
                }
                case ReadState::body_chunked_init: {
                    ctx.save_remain_chunk_size(0);
                    if (ctx.body_type() == BodyType::chunked_content_length &&
                        ctx.is_flag(ReadFlag::consistent_chunked_content_length)) {
                        ctx.save_remain_content_length(ctx.content_length());
                    }
                    change_state(ReadState::body_chunked_size);
                    [[fallthrough]];  // fallthrough
                }
                case ReadState::body_chunked_size: {
                    auto size_read = ctx.remain_chunk_size();
                    while (true) {
                        if (seq.eos()) {
                            ctx.save_remain_chunk_size(size_read);
                            save_pos();
                            return BodyResult::incomplete;
                        }
                        if (!number::is_in_byte_range(seq.current()) || !number::is_hex(seq.current())) {
                            break;
                        }
                        size_read *= 16;
                        size_read += number::number_transform[int(seq.current())];
                        seq.consume();
                    }
                    ctx.save_remain_chunk_size(size_read);
                    if (ctx.body_type() == BodyType::chunked_content_length &&
                        ctx.is_flag(ReadFlag::consistent_chunked_content_length)) {
                        // if chunked_content_length, check size is not over content-length
                        if (size_read > ctx.remain_content_length()) {
                            return BodyResult::length_mismatch;
                        }
                    }
                    change_state(ReadState::body_chunked_extension_init);
                    [[fallthrough]];  // fallthrough
                }
                case ReadState::body_chunked_extension_init: {
                    // skip BWS
                    while (true) {
                        if (seq.eos()) {
                            save_pos();
                            return BodyResult::incomplete;
                        }
                        if (!is_tab_or_space(seq.current())) {
                            break;
                        }
                        seq.consume();
                    }
                    // any way here, least 1 byte is remain
                    if (!seq.consume_if(';')) {
                        // if BWS is not followed by ';', it's invalid
                        if (ctx.start_pos() - seq.rptr != 0) {
                            return BodyResult::bad_space;
                        }
                        change_state(ReadState::body_chunked_size_eol_one_byte);
                        break;  // jump to next state
                    }
                    change_state(ReadState::body_chunked_extension);
                    [[fallthrough]];  // fallthrough
                }
                case ReadState::body_chunked_extension: {
                    while (true) {
                        if (seq.eos()) {
                            save_pos();
                            return BodyResult::incomplete;
                        }
                        if (is_line(seq.current())) {
                            break;
                        }
                        seq.consume();
                    }
                    // TODO(on-keyday): trim BWS?
                    range_to_string_or_call(seq, chunked_extension, {ctx.start_pos(), seq.rptr});
                    change_state(ReadState::body_chunked_size_eol_one_byte);
                    [[fallthrough]];  // fallthrough
                }
                case ReadState::body_chunked_size_eol_one_byte:
                case ReadState::body_chunked_size_eol_two_byte: {
                    if (!read_eol_with_flag(ctx, seq, ReadState::body_chunked_size_eol_one_byte, ReadState::body_chunked_size_eol_two_byte, ReadState::body_chunked_data_init)) {
                        return ctx.is_resumable() ? BodyResult::incomplete : BodyResult::bad_line;
                    }
                    [[fallthrough]];  // fallthrough
                }
                case ReadState::body_chunked_data_init: {
                    // if chunk size is 0, it's end of chunked data
                    if (ctx.remain_chunk_size() == 0) {
                        if (ctx.body_type() == BodyType::chunked_content_length &&
                            ctx.is_flag(ReadFlag::consistent_chunked_content_length)) {
                            // if chunked_content_length, check remain content-length is 0
                            if (ctx.remain_content_length() != 0) {
                                return BodyResult::length_mismatch;
                            }
                        }
                        if (ctx.is_flag(ReadFlag::not_strict_trailer) ||
                            ctx.has_trailer()) {
                            change_state(ReadState::trailer_init);
                            return BodyResult::full;
                        }
                        change_state(ReadState::trailer_last_eol_one_byte);
                        break;  // jump to next state
                    }
                    change_state(ReadState::body_chunked_data);
                    [[fallthrough]];  // fallthrough
                }
                case ReadState::body_chunked_data: {
                    if (seq.eos()) {
                        save_pos();
                        return BodyResult::incomplete;
                    }
                    auto base_pos = seq.rptr;
                    auto remain_chunk = ctx.remain_chunk_size();
                    auto remain_input = seq.remain();
                    if (remain_input < remain_chunk) {
                        range_to_string_or_call(seq, body, {seq.rptr, seq.rptr + remain_input});
                        seq.rptr = base_pos + remain_input;
                        ctx.save_remain_chunk_size(remain_chunk - remain_input);
                        if (ctx.body_type() == BodyType::chunked_content_length) {
                            ctx.save_remain_content_length(ctx.remain_content_length() - remain_input);
                        }
                        save_pos();
                        return BodyResult::incomplete;
                    }
                    range_to_string_or_call(seq, body, {seq.rptr, seq.rptr + remain_chunk});
                    seq.rptr = base_pos + remain_chunk;
                    ctx.save_remain_chunk_size(0);
                    if (ctx.body_type() == BodyType::chunked_content_length) {
                        ctx.save_remain_content_length(ctx.remain_content_length() - remain_chunk);
                    }
                    change_state(ReadState::body_chunked_data_eol_one_byte);
                    if (ctx.is_flag(ReadFlag::suspend_on_chunked)) {
                        save_pos();
                        return BodyResult::incomplete;
                    }
                    [[fallthrough]];  // fallthrough
                }
                case ReadState::body_chunked_data_eol_one_byte:
                case ReadState::body_chunked_data_eol_two_byte: {
                    if (!read_eol_with_flag(ctx, seq, ReadState::body_chunked_data_eol_one_byte, ReadState::body_chunked_data_eol_two_byte, ReadState::body_chunked_size)) {
                        return ctx.is_resumable() ? BodyResult::incomplete : BodyResult::bad_line;
                    }
                    break;  // jump to next state
                }
                case ReadState::trailer_last_eol_one_byte:  // for chunked body without trailer
                case ReadState::trailer_last_eol_two_byte: {
                    if (!read_eol_with_flag(ctx, seq, ReadState::trailer_last_eol_one_byte, ReadState::trailer_last_eol_two_byte, ReadState::trailer_init)) {
                        return ctx.is_resumable() ? BodyResult::incomplete : BodyResult::bad_line;
                    }
                    change_state(ReadState::body_end);
                    return BodyResult::full;
                }
                case ReadState::body_end: {
                    return BodyResult::full;  // already end
                }
                default: {
                    return BodyResult::invalid_state;
                }
            }
        }
    }

    /*
auto base_pos = seq.rptr;
if (info.type == BodyType::chunked) {
  bool read_chunk = false;
  while (true) {
      if (seq.eos()) {
          return BodyReadResult::incomplete;
      }
      switch (info.chunk_state) {
          case ChunkReadState::chunk_size: {
              base_pos = seq.rptr;
              size_t num = 0;
              auto e = number::parse_integer(seq, num, 16);
              if (e != number::NumError::none) {
                  return BodyReadResult::invalid;
              }
              if (seq.remain() < 2) {
                  seq.rptr = base_pos;
                  return BodyReadResult::incomplete;
              }
              if (!seq.seek_if("\r\n")) {
                  return BodyReadResult::invalid;
              }
              info.expect = num;
              info.chunk_state = ChunkReadState::chunk_data;
              [[fallthrough]];  // fallthrough
          }
          case ChunkReadState::chunk_data: {
              if (seq.remain() < info.expect) {
                  return BodyReadResult::incomplete;
              }
              if (!strutil::read_n(result, seq, info.expect)) {
                  return BodyReadResult::invalid;
              }
              info.chunk_state = ChunkReadState::chunk_data_line;
              read_chunk = true;
              [[fallthrough]];  // fallthrough
          }
          case ChunkReadState::chunk_data_line: {
              if (seq.remain() < 2) {
                  return read_chunk ? BodyReadResult::chunk_read : BodyReadResult::incomplete;
              }
              if (!seq.seek_if("\r\n")) {
                  return BodyReadResult::invalid;
              }
              if (info.expect == 0) {
                  info.chunk_state = ChunkReadState::chunk_end;
                  return BodyReadResult::full;
              }
              info.chunk_state = ChunkReadState::chunk_size;
              break;
          }
          case ChunkReadState::chunk_end: {
              return BodyReadResult::full;
          }
      }
  }
}
else if (info.type == BodyType::content_length) {
  if (seq.remain() < info.expect) {
      return BodyReadResult::incomplete;
  }
  if (!strutil::read_n(result, seq, info.expect)) {
      return BodyReadResult::invalid;
  }
  return BodyReadResult::full;
}
else {
  if (!strutil::read_all(result, seq)) {
      return BodyReadResult::invalid;
  }
  return BodyReadResult::best_effort;
}
*/

    /*
    constexpr auto body_info_preview(BodyType& type, size_t& expect) {
        return [&](auto& key, auto& value) {
            if (strutil::equal(key, "transfer-encoding", strutil::ignore_case()) &&
                strutil::contains(value, "chunked")) {
                type = BodyType::chunked;
            }
            else if (type == BodyType::no_info &&
                     strutil::equal(key, "content-length", strutil::ignore_case())) {
                auto seq = make_ref_seq(value);
                number::parse_integer(seq, expect);
                type = BodyType::content_length;
            }
        };
    }
    */

    namespace internal {
        constexpr size_t size_or_strlen(auto&& data) {
            if constexpr (std::is_pointer_v<std::decay_t<decltype(data)>>) {
                return data ? futils::strlen(data) : 0;
            }
            else {
                return data.size();
            }
        }
    }  // namespace internal

    template <class Buf, class Data>
    constexpr BodyResult render_length_body(WriteContext& ctx, Buf& buf, Data&& data) {
        if (ctx.state() != WriteState::content_length_body) {
            return BodyResult::invalid_state;
        }
        size_t size = internal::size_or_strlen(data);
        if (ctx.remain_content_length() < size) {
            return BodyResult::length_mismatch;
        }
        strutil::append(buf, data);
        ctx.save_remain_content_length(ctx.remain_content_length() - size);
        if (ctx.remain_content_length() == 0) {
            ctx.set_state(WriteState::end);
            return BodyResult::full;
        }
        return BodyResult::incomplete;
    }

    template <class Buf, class Data, class Extension = const char*>
    constexpr BodyResult render_chunked_body(WriteContext& ctx, Buf& buf, Data&& data, Extension&& extension = "") {
        if (ctx.state() != WriteState::chunked_body &&
            ctx.state() != WriteState::content_length_chunked_body) {
            return BodyResult::invalid_state;
        }
        size_t size = internal::size_or_strlen(data);
        if (ctx.state() == WriteState::content_length_chunked_body) {
            if (ctx.remain_content_length() < size) {
                return BodyResult::length_mismatch;
            }
            ctx.save_remain_content_length(ctx.remain_content_length() - size);
        }
        number::to_string(buf, size, 16);
        if (auto ext_size = internal::size_or_strlen(extension); ext_size > 0) {
            strutil::append(buf, ";");
            strutil::append(buf, extension);
        }
        strutil::append(buf, "\r\n");
        if (size == 0) {
            if (ctx.state() == WriteState::content_length_chunked_body) {
                if (ctx.remain_content_length() != 0) {
                    return BodyResult::length_mismatch;
                }
            }
            if (!ctx.has_trailer()) {
                strutil::append(buf, "\r\n");
                ctx.set_state(WriteState::end);
            }
            else {
                ctx.set_state(WriteState::trailer);
            }
            return BodyResult::full;
        }
        else {
            strutil::append(buf, data);
            strutil::append(buf, "\r\n");
            return BodyResult::incomplete;
        }
    }

    template <class Buf, class Data>
    constexpr BodyResult render_body(WriteContext& ctx, Buf& buf, Data&& data) {
        switch (ctx.state()) {
            case WriteState::best_effort_body:
                strutil::append(buf, data);
                return BodyResult::best_effort;
            case WriteState::content_length_body:
                return render_length_body(ctx, buf, data);
            case WriteState::chunked_body:
            case WriteState::content_length_chunked_body:
                return render_chunked_body(ctx, buf, data);
            default:
                return BodyResult::invalid_state;
        }
    }

    namespace test {
        constexpr bool run_test_case(const char* seq_data, BodyType body_type, std::size_t content_length,
                                     BodyResult expected_result, const char* expected_body, const char* expected_ext = nullptr) {
            ReadContext ctx;
            // bad behavior, but for test
            ctx.set_flag(ReadFlag::consistent_chunked_content_length);
            auto seq = make_ref_seq(seq_data);
            ctx.set_body_info(body_type, content_length);
            auto report = [](const char* msg, auto... ctx) {
                throw msg;
            };

            number::Array<std::uint8_t, 100> body{};
            number::Array<std::uint8_t, 100> ext{};
            auto res = read_body(ctx, seq, body, ext);
            if (res != expected_result) {
                report("Result not equal", res, expected_result);
            }
            if (res != BodyResult::full && res != BodyResult::best_effort) {
                return true;
            }
            if (!strutil::equal(body, expected_body) ||
                (expected_ext && !strutil::equal(ext, expected_ext))) {
                report("Test case failed", res, seq.rptr, ctx, body, ext);
            }
            return true;
        }
        constexpr bool test_read_body() {
            // normal cases
            run_test_case("test data", BodyType::no_info, 0, BodyResult::best_effort, "test data");
            run_test_case("12345678901234567890", BodyType::content_length, 10, BodyResult::full, "1234567890");
            run_test_case("10\r\n1234567890123456\r\n0\r\n\r\n", BodyType::chunked, 0, BodyResult::full, "1234567890123456");
            run_test_case("10\r\n1234567890123456\r\n0\r\n", BodyType::chunked, 0, BodyResult::incomplete, "1234567890123456");

            // on rfc, this is invalid, but it's valid in this implementation
            run_test_case("3;ext\r\nabc\r\n0\r\n\r\n", BodyType::chunked_content_length, 3, BodyResult::full, "abc", "ext");
            run_test_case("3;\r\nabc\r\n0\r\n\r\n", BodyType::chunked_content_length, 3, BodyResult::full, "abc", "");

            // invalid cases
            run_test_case("10\r\n123456789012345\r\n0\r\n", BodyType::chunked_content_length, 16, BodyResult::bad_line, "");
            run_test_case("11\r\n12345678901234567\r\n0\r\n", BodyType::chunked_content_length, 16, BodyResult::length_mismatch, "");
            run_test_case("10\r\n1234567890123456\r\n", BodyType::chunked, 0, BodyResult::incomplete, "");
            run_test_case("10\r\n1234567890123456\r\n0", BodyType::chunked, 0, BodyResult::incomplete, "");
            run_test_case("10\r\n1234567890123456\r\n0\r", BodyType::chunked, 0, BodyResult::incomplete, "");
            run_test_case("10\r\n1234567890123456\r\n0\n", BodyType::chunked, 0, BodyResult::bad_line, "");
            run_test_case("F\r\n123456789012345\r\n0\r\n", BodyType::chunked_content_length, 16, BodyResult::length_mismatch, "");
            run_test_case("10 \r\n1234567890123456\r\n0\r\n", BodyType::chunked_content_length, 16, BodyResult::bad_space, "");
            return true;
        }

        static_assert(test_read_body());

        constexpr bool test_render_body() {
            auto do_render = [&](WriteState state, const char* data, size_t content_length, BodyResult expected_result, const char* expected_body) {
                WriteContext ctx;
                ctx.set_state(state);
                number::Array<std::uint8_t, 100> buf{};
                ctx.save_remain_content_length(content_length);
                auto res = render_body(ctx, buf, data);
                if (res != expected_result) {
                    throw "Result not equal";
                }
                if (!strutil::equal(buf, expected_body)) {
                    throw "Test case failed";
                }
                return true;
            };
            do_render(WriteState::best_effort_body, "test data", 0 /*no effect*/, BodyResult::best_effort, "test data");
            do_render(WriteState::content_length_body, "1234567890", 10, BodyResult::full, "1234567890");
            do_render(WriteState::content_length_body, "1234567890", 5, BodyResult::length_mismatch, "");
            do_render(WriteState::content_length_body, "1234567890", 11, BodyResult::incomplete, "1234567890");
            do_render(WriteState::chunked_body, "1234567890", 0, BodyResult::incomplete, "a\r\n1234567890\r\n");
            do_render(WriteState::content_length_chunked_body, "1234567890", 10, BodyResult::incomplete, "a\r\n1234567890\r\n");
            do_render(WriteState::chunked_body, "", 0, BodyResult::full, "0\r\n\r\n");
            return true;
        }

        static_assert(test_render_body());

    }  // namespace test

}  // namespace futils::fnet::http1::body
