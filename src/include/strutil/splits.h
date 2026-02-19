/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// splits - split method
#pragma once

#include "../core/sequencer.h"

#include "../wrap/light/string.h"
#include "../wrap/light/vector.h"

namespace futils {
    namespace strutil {

        template <class T, class U>
        concept has_push_back = requires(T t, U u) {
            t.push_back(u);
        };

        template <class S>
        concept has_substr = requires(S s) {
            s.substr(size_t(1), size_t(1));
        };

        template <class T, class String, class Sep, template <class...> class Vec>
        void split(Vec<String>& result, Sequencer<T>& input, Sep&& sep, size_t n = ~0, bool ignore_after_n = false) {
            String current;
            size_t count = 0;
            constexpr bool push_back_mode = has_push_back<decltype(current), decltype(input.current())>;
            constexpr bool substr_mode = has_substr<decltype(input.buf.buffer)> && !push_back_mode;
            static_assert(push_back_mode || substr_mode, "String type must have push_back or substr method");
            size_t prev_pos = input.rptr;
            auto substr = [&](auto& s, size_t sep) {
                if constexpr (substr_mode) {
                    s = input.buf.buffer.substr(prev_pos, input.rptr - prev_pos - sep);
                }
            };
            while (!input.eos()) {
                auto pre_sep = input.rptr;
                if (count < n && input.seek_if(sep)) {
                    substr(current, input.rptr - pre_sep);
                    result.push_back(std::move(current));
                    count++;
                    prev_pos = input.rptr;
                    if (count >= n && ignore_after_n) {
                        break;
                    }
                    continue;
                }
                if constexpr (push_back_mode) {
                    current.push_back(input.current());
                }
                input.consume();
            }
            if (!ignore_after_n && input.rptr > prev_pos) {
                substr(current, 0);
                result.push_back(std::move(current));
            }
        }

        template <class T, class String, template <class...> class Vec>
        void lines(Vec<String>& result, Sequencer<T>& input, bool needtk = false) {
            String current;
            size_t prev_pos = input.rptr;
            constexpr bool push_back_mode = has_push_back<decltype(current), decltype(input.current())>;
            constexpr bool substr_mode = has_substr<decltype(input.buf.buffer)> && !push_back_mode;
            static_assert(push_back_mode || substr_mode, "String type must have push_back or substr method");
            auto substr = [&](auto& s, size_t line_char_count) {
                if constexpr (substr_mode) {
                    if (needtk) {
                        s = input.buf.buffer.substr(prev_pos, input.rptr - prev_pos);
                    }
                    else {
                        s = input.buf.buffer.substr(prev_pos, input.rptr - prev_pos - line_char_count);
                    }
                }
            };
            while (!input.eos()) {
                if (input.seek_if("\r\n")) {
                    if constexpr (push_back_mode) {
                        if (needtk) {
                            current.push_back('\r');
                            current.push_back('\n');
                        }
                    }
                    else if constexpr (substr_mode) {
                        substr(current, 2);
                    }
                    result.push_back(std::move(current));
                    prev_pos = input.rptr;
                }
                else if (input.seek_if("\r")) {
                    if constexpr (push_back_mode) {
                        if (needtk) {
                            current.push_back('\r');
                        }
                    }
                    else if constexpr (substr_mode) {
                        substr(current, 1);
                    }
                    result.push_back(std::move(current));
                    prev_pos = input.rptr;
                }
                else if (input.seek_if("\n")) {
                    if constexpr (push_back_mode) {
                        if (needtk) {
                            current.push_back('\n');
                        }
                    }
                    else if constexpr (substr_mode) {
                        substr(current, 1);
                    }
                    result.push_back(std::move(current));
                    prev_pos = input.rptr;
                }
                else {
                    if constexpr (push_back_mode) {
                        current.push_back(input.current());
                    }
                    input.consume();
                }
            }
            if (input.rptr > prev_pos) {
                if constexpr (substr_mode) {
                    substr(current, 0);
                }
                result.push_back(std::move(current));
            }
        }

        template <class String = wrap::string, template <class...> class Vec = wrap::vector, class T, class Sep>
        Vec<String> split(T&& input, Sep&& sep, size_t n = ~0, bool igafter = false) {
            Vec<String> result;
            Sequencer<buffer_t<T&>> intmp(input);
            split(result, intmp, sep, n, igafter);
            return result;
        }

        template <class String = wrap::string, template <class...> class Vec = wrap::vector, class T>
        Vec<String> lines(T&& input, bool needtk = false) {
            Vec<String> result;
            Sequencer<buffer_t<T&>> intmp(input);
            lines(result, intmp, needtk);
            return result;
        }
    }  // namespace strutil
}  // namespace futils
