/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../conv_method.h"
#include "../../../helper/append_charsize.h"

namespace utils {
    namespace utf {
        constexpr std::uint8_t u8repchar[] = {0xEF, 0xBF, 0xBD};
        constexpr std::uint16_t u16repchar = 0xFFFD;

        namespace internal {
            template <class U, size_t val>
            struct DecideOutLen {
                static constexpr size_t size = val;
            };

            template <class U>
            struct DecideOutLen<U, 0> {
                static constexpr size_t size = helper::append_size_v<U>;
            };

            template <size_t insize = 0, size_t outsize = 0, class T, class U>
            constexpr UTFErr convert_impl2(Sequencer<T>& input, U& output, auto&& samesize_traits, auto&& fail_traits) {
                constexpr auto in_size = insize == 0 ? sizeof(input.current()) : insize;
                constexpr auto out_size = DecideOutLen<U, outsize>::size;
                auto call_fail = [&](auto& ok) {
                    return fail_traits(ok, input, in_size, output, out_size);
                };
                if constexpr (in_size == out_size) {
                    if (auto ok = samesize_traits(input, in_size, output, out_size); !ok) {
                        return call_fail(ok);
                    }
                    return UTFError::none;
                }
                else if constexpr (in_size == 1) {
                    static_assert(out_size == 2 || out_size == 4, "expect convertion to UTF16 or UTF32");
                    if constexpr (out_size == 4) {
                        char32_t value;
                        if (auto ok = utf8_to_utf32(input, value); !ok) {
                            return call_fail(ok);
                        }
                        output.push_back(value);
                        return UTFError::none;
                    }
                    else {
                        if (auto ok = utf8_to_utf16(input, output); !ok) {
                            return call_fail(ok);
                        }
                        return UTFError::none;
                    }
                }
                else if constexpr (in_size == 2) {
                    static_assert(out_size == 1 || out_size == 4, "expect convertion to UTF8 or UTF32");
                    if constexpr (out_size == 4) {
                        char32_t value;
                        if (auto ok = utf16_to_utf32(input, value); !ok) {
                            return call_fail(ok);
                        }
                        output.push_back(value);
                        return UTFError::none;
                    }
                    else {
                        if (auto ok = utf16_to_utf8(input, output); !ok) {
                            return call_fail(ok);
                        }
                        return UTFError::none;
                    }
                }
                else if constexpr (in_size == 4) {
                    static_assert(out_size == 1 || out_size == 2, "expect convertion to UTF8 or UTF16");
                    if constexpr (out_size == 2) {
                        if (auto ok = utf32_to_utf16(input.current(), output); !ok) {
                            return call_fail(ok);
                        }
                        input.consume();
                        return UTFError::none;
                    }
                    else {
                        if (auto ok = utf32_to_utf8(input.current(), output); !ok) {
                            return call_fail(ok);
                        }
                        input.consume();
                        return UTFError::none;
                    }
                }
                else {
                    static_assert(
                        (in_size == 1 || in_size == 2 || in_size == 4) &&
                            (out_size == 1 || out_size == 2 || out_size == 4),
                        "expect 1 or 2 or 4 char size");
                }
            }

            constexpr auto on_samesize_none() {
                return [](auto& input, size_t insize, auto& output, size_t outsize) -> UTFErr {
                    output.push_back(input.current());
                    input.consume();
                    return UTFError::none;
                };
            }

            constexpr auto on_samesize_code_point() {
                return [](auto& input, size_t insize, auto& output, size_t outsize) -> UTFErr {
                    if (insize == 1) {
                        auto base = input.rptr;
                        std::uint32_t value;
                        if (auto ok = utf8_to_utf32(input, value); !ok) {
                            return ok;
                        }
                        auto len = input.rptr - base;
                        input.rptr = base;
                        for (auto i = 0; i < len; i++) {
                            output.push_back(input.current());
                            input.consume();
                        }
                        return UTFError::none;
                    }
                    else if (insize == 2) {
                        auto base = input.rptr;
                        std::uint32_t value;
                        if (auto ok = utf16_to_utf32(input, value); !ok) {
                            return ok;
                        }
                        auto len = input.rptr - base;
                        input.rptr = base;
                        for (auto i = 0; i < len; i++) {
                            output.push_back(input.current());
                            input.consume();
                        }
                        return UTFError::none;
                    }
                    if (std::uint32_t(input.current()) >= 0x110000) {
                        return UTFError::utf32_out_of_range;
                    }
                    output.push_back(input.current());
                    return UTFError::none;
                };
            }

            constexpr auto on_fail_none() {
                return [](auto ok, auto&&...) {
                    return ok;
                };
            }

            constexpr auto on_fail_unsafe() {
                return [](auto ok, auto& input, size_t, auto& output, size_t) {
                    output.push_back(input.current());
                    input.consume();
                    return UTFError::none;
                };
            }

            constexpr auto write_repchar(auto& output, size_t outsize) {
                if (outsize == 1) {
                    output.push_back(u8repchar[0]);
                    output.push_back(u8repchar[1]);
                    output.push_back(u8repchar[2]);
                }
                else {
                    auto val = u16repchar;
                    output.push_back(val);
                }
            }

            constexpr auto on_fail_replace() {
                return [](UTFErr err, auto& input, size_t insize, auto& output, size_t outlen) -> UTFErr {
                    if (err.is(UTFError::utf16_invalid_surrogate) ||
                        err.is(UTFError::utf8_illformed_sequence)) {
                        if (insize == 1) {
                            auto len = utf8_expect_len_from_first_byte(input.current());
                            input.consume(len);
                            write_repchar(output, outlen);
                        }
                        else if (insize == 2) {
                            input.consume(2);
                            write_repchar(output, outlen);
                        }
                        return UTFError::none;
                    }
                    return err;
                };
            }

        }  // namespace internal
    }      // namespace utf
}  // namespace utils
