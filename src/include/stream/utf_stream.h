/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// utf_stream - read any utf string
#pragma once
#include "stream.h"
#include <helper/view.h>
#include <number/array.h>
#include <utf/conv_method.h>
#include <number/to_string.h>
#include "token_stream.h"

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
                number::Array<5, std::uint8_t, true> utf8;
                size_t r;
                while (true) {
                    auto pos = input.pos();
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
                    auto view = helper::SizedView((const std::uint8_t*)ptr, r);
                    // here seq.rptr == 0
                    auto seq = make_ref_seq(view);
                    auto should_stop_seq = [&](size_t e) {
                        const auto s = seq.rptr;
                        for (auto i = s; i < s + e; i++) {
                            utf8 = {};
                            utf8.push_back(ptr[i]);
                            if (should_stop(ptr[i], utf8.c_str(), utf8.size())) {
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
                    while (!seq.eos()) {
                        char32_t c;
                        auto len = utf::expect_len(seq.current());
                        if (!utf::utf8_to_utf32(seq, c)) {
                            if (seq.remain() < len) {
                                break;
                            }
                            if (perr) {
                                invalid_utf_error err;
                                err.pos = pos + seq.rptr;
                                err.input_size = len;
                                for (auto i = 0; i < len; i++) {
                                    err.inputs[i] = view.ptr[seq.rptr];
                                }
                                *perr = std::move(err);
                            }
                            return false;
                        }
                        utf8 = {};
                        for (auto i = 0; i < len; i++) {
                            utf8.push_back(seq.current(-len + i));
                        }
                        if (should_stop(c, utf8.c_str(), utf8.size())) {
                            input.offset_seek(seq.rptr);
                            return true;
                        }
                    }
                    // seek input with size read by this loop
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

                bool ok(UTFStat* stat) {
                    return fn(stat->C, stat->index);
                }

                Token endok() {
                    return {};
                }
            };

            template <class Fn>
            struct RecPrevChecker {
                char32_t rec;
                Fn fn;
                RecPrevChecker(Fn&& f)
                    : fn(std::move(f)) {}
                bool ok(UTFStat* stat) {
                    if (stat->index == 0) {
                        rec = 0;
                    }
                    if (!fn(stat->C, stat->index, rec)) {
                        return false;
                    }
                    rec = stat->C;
                    return true;
                }

                Token endok() {
                    return {};
                }
            };
            template <class Check, class Str>
            auto default_utf_callback(Check& check, Str& str, size_t& offset_base, size_t& offset, size_t& index) {
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
                    if (!check.ok(&stat)) {
                        return true;
                    }
                    offset += size;
                    index++;
                    helper::append(str, helper::SizedView{u8, size});
                    return false;
                };
            }

            template <class String, class Checker>
            struct UtfParser {
                Checker check;
                const char* expect;
                Token parse(Input& input) {
                    Token err;
                    String str;
                    auto pos = input.pos();
                    auto offset_base = pos;
                    size_t offset = 0;
                    size_t index = 0;
                    auto ok = read_utf_string(input, &err,
                                              default_utf_callback(check, str, offset_base, offset, index));
                    if (!ok) {
                        return err;
                    }
                    err = check.endok();
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
