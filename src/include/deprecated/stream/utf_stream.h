/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// utf_stream - read any utf string
#pragma once
#include "stream.h"
#include <number/array.h>
#include <utf/conv_method.h>
#include <number/to_string.h>
#include "token_stream.h"
#include <view/charvec.h>

namespace utils {
    namespace parser {
        namespace stream {

            struct invalid_utf_error {
                size_t pos;
                size_t input_size;
                std::uint8_t inputs[4];
                void error(PB) {}
                void token(PB) {}
                Error err() {
                    return *this;
                }
                TokenInfo info() {
                    return TokenInfo{
                        .pos = pos,
                        .has_err = true,

                    };
                }
            };

            template <class Fn>
            bool read_utf_string(Input& input, Token* perr, Fn&& should_stop) {
                char tmpbuf[20];
                number::Array<std::uint8_t, 5, true> utf8;
                size_t r;
                while (true) {
                    //  load raw binary data
                    auto ptr = input.peek(tmpbuf, 20, &r);
                    if (r == 0) {
                        return true;
                    }
                    // compare pointer and ensure size
                    if (ptr == tmpbuf && r > 20) {
                        if (perr) {
                            *perr = simpleErrToken{"size from input.peek() is invalid"};
                        }
                        return false;
                    }
                    auto view = view::CharVec((const std::uint8_t*)ptr, r);
                    // here seq.rptr == 0
                    auto seq = make_ref_seq(view);
                    auto should_stop_seq = [&](size_t e) {
                        const auto s = seq.rptr;
                        for (auto i = s; i < s + e; i++) {
                            utf8 = {};
                            utf8.push_back(view.ptr[i]);
                            if (should_stop(view.ptr[i], utf8.c_str(), utf8.size())) {
                                input.offset_seek(s + i);
                                return true;
                            }
                        }
                        seq.consume(e);
                        return false;
                    };
                    // ascii shortcut
                    // reading ascii without utf converting
                    while (seq.remain() >= 8) {
                        auto shift = [&](size_t s) -> std::uint64_t {
                            return std::uint64_t(view.ptr[seq.rptr + s]) << (8 * s);
                        };
                        // get 8byte == sizeof(std::uint64_t)
                        auto val = shift(0) | shift(1) | shift(2) | shift(3) |
                                   shift(4) | shift(5) | shift(6) | shift(7);
                        constexpr auto validate = 0x80'80'80'80'80'80'80'80;
                        constexpr auto validat2 = 0x00'00'00'00'80'80'80'80;
                        // validate input being ascii range
                        // if v & (0x80<<(8*offset)) == 0 then
                        // character is in ascii range
                        // else then character includes utf multibyte
                        if (val & validate) {
                            // fallback validation
                            // for 4 byte
                            if (val & validat2) {
                                break;
                            }
                            if (should_stop_seq(4)) {
                                return true;
                            }
                            break;
                        }
                        if (should_stop_seq(8)) {
                            return true;
                        }
                    }
                    // convertion loop
                    while (!seq.eos()) {
                        char32_t c;
                        // for prepare utf8 and error
                        auto len = utf::expect_len(seq.current());
                        // if len==0 then convertion will failure
                        if (!len || !utf::utf8_to_utf32(seq, c)) {
                            if (seq.remain() < len) {
                                // convertion is failed by lack of bytes
                                // break to next loop
                                break;
                            }
                            if (perr) {
                                invalid_utf_error err;
                                err.pos = input.pos() + seq.rptr;
                                err.input_size = len == 0 ? 1 : len;
                                for (auto i = 0; i < err.input_size; i++) {
                                    err.inputs[i] = view.ptr[seq.rptr];
                                }
                                *perr = std::move(err);
                            }
                            return false;
                        }
                        // prepare utf8 string
                        // here seq.rptr-len is position before convertion
                        utf8 = {};
                        for (auto i = 0; i < len; i++) {
                            utf8.push_back(seq.current(-len + i));
                        }
                        // if should stop returns true then
                        // seek input and return
                        if (should_stop(c, utf8.c_str(), utf8.size())) {
                            // seq.rptr-len is ok offset
                            input.offset_seek(seq.rptr - len);
                            return true;
                        }
                    }
                    // seek input with size read by this loop
                    // here seq.rptr is offset from input.pos()
                    input.offset_seek(seq.rptr);
                }
            }

            struct UTFStat {
                char32_t C;
                size_t index;
                const std::uint8_t* utf8;
                size_t len;
                size_t actual_offset;
                InputStat input;
            };

            template <class Fn>
            struct SingleChecker {
                Fn fn;
                SingleChecker(Fn&& f)
                    : fn(std::move(f)) {}

                bool ok(UTFStat* stat, auto& str) {
                    return fn(stat->C, stat->index);
                }

                ErrorToken endok(Input&, auto& str) {
                    return {};
                }
            };

            template <class Fn>
            struct RecPrevChecker {
                char32_t rec;
                Fn fn;
                RecPrevChecker(Fn&& f)
                    : fn(std::move(f)) {}
                bool ok(UTFStat* stat, auto& str) {
                    if (stat->index == 0) {
                        rec = 0;
                    }
                    if (!fn(stat->C, stat->index, rec)) {
                        return false;
                    }
                    rec = stat->C;
                    return true;
                }

                ErrorToken endok(Input&, auto& str) {
                    return {};
                }
            };
            template <class Check, class Str>
            auto default_utf_callback(Input& input, Check& check, Str& str, size_t& offset_base, size_t& offset, size_t& index) {
                return [&](char32_t c, const std::uint8_t* u8, size_t size) {
                    UTFStat stat;
                    stat.input = input.info();
                    if (stat.input.pos != offset_base) {
                        offset_base = input.pos();
                        offset = 0;
                    }
                    stat.C = c;
                    stat.index = index;
                    stat.utf8 = u8;
                    stat.len = size;
                    stat.actual_offset = offset;
                    if (!check.ok(&stat, str)) {
                        return true;
                    }
                    offset += size;
                    index++;
                    helper::append(str, view::CharVec{u8, size});
                    return false;
                };
            }

            template <class Str, class Check>
            ErrorToken read_default_utf(Input& input, Str& str, Check& check) {
                auto offset_base = input.pos();
                size_t offset = 0;
                size_t index = 0;
                ErrorToken err;
                if (!read_utf_string(input, &err,
                                     default_utf_callback(input, check, str, offset_base, offset, index))) {
                    return err;
                }
                err = check.endok(input, str);
                if (has_err(err)) {
                    return err;
                }
                return {};
            }

            template <class String, class Checker>
            struct UtfParser {
                Checker check;
                const char* expect;
                Token parse(Input& input) {
                    String str;
                    auto pos = input.pos();
                    auto err = read_default_utf(input, str, check);
                    if (has_err(err)) {
                        return err;
                    }
                    return SimpleCondToken<String>{std::move(str), pos, expect};
                }
            };

            template <class Str, class Check>
            auto make_utf(const char* expect, Check check) {
                return UtfParser<Str, Check>{std::move(check), expect};
            }

        }  // namespace stream
    }      // namespace parser
}  // namespace utils
