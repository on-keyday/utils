/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../core/sequencer.h"
#include "../view/iovec.h"
#include "../strutil/append.h"
#include "../helper/pushbacker.h"

// for test
#include "../binary/writer.h"
#include "../strutil/equal.h"

namespace utils {
    namespace env {
        template <class T>
        constexpr bool has_key_value(Sequencer<T>& seq) {
            while (!seq.eos()) {
                if (seq.consume_if('=')) {
                    return true;
                }
                seq.consume();
            }
            return false;
        }

        template <class C>
        constexpr std::pair<view::basic_rvec<C>, view::basic_rvec<C>> get_key_value(view::basic_rvec<C> kv, bool allow_front_eq = false) {
            auto seq = make_cpy_seq(kv);
            if (allow_front_eq) {
                // on windows some environment variables has `=` prefix
                seq.consume();  // so ignore it
            }
            if (has_key_value(seq)) {
                return {seq.buf.buffer.substr(0, seq.rptr - 1), seq.buf.buffer.substr(seq.rptr)};
            }
            return {};
        }

        namespace test {
            constexpr auto key_value = get_key_value(view::basic_rvec<char>{"key=value"});
        }

        constexpr bool is_shell_var_char(auto c) noexcept {
            return c == '_' ||
                   '0' <= c && c <= '9' ||
                   'A' <= c && c <= 'Z' ||
                   'a' <= c && c <= 'z';
        }

        constexpr bool is_special_char(auto c) noexcept {
            switch (c) {
                case '*':
                case '#':
                case '$':
                case '@':
                case '!':
                case '?':
                case '-':
                    return true;
                default:
                    break;
            }
            if ('0' <= c && c <= '9') {
                return true;
            }
            return false;
        }

        namespace internal {
            template <class T>
            concept has_reserve = requires(T t) {
                { t.reserve(size_t{}) };
            };
        }

        template <class T>
        constexpr bool read_varchar_unix(auto&& out, Sequencer<T>& src, auto&& expand) {
            bool called = false;
            bool res = true;
            auto read = [&](auto&& o) {
                if (called) {
                    return false;
                }
                called = true;
                if (src.current() == '{') {
                    src.consume();  // consume `{`
                    if (src.current() == '}') {
                        src.consume();  // `${}` invalid syntax, nothing to read
                        res = false;
                        return false;
                    }
                    const auto ptr = src.rptr;
                    while (!src.eos() && src.current() != '}') {
                        src.consume();
                    }
                    if (src.eos()) {
                        // `${`: invalid syntax, nothing to read
                        src.rptr = ptr;
                        res = false;
                        return false;
                    }
                    const auto end_ptr = src.rptr;
                    src.consume();  // consume '}'
                    if constexpr (internal::has_reserve<decltype(o)>) {
                        o.reserve(end_ptr - ptr);
                    }
                    for (auto i = ptr; i < end_ptr; i++) {
                        o.push_back(src.buf.buffer[i]);
                    }
                    return true;
                }
                if (is_special_char(src.current())) {
                    o.push_back(src.current());
                    src.consume();
                    return true;
                }
                // if !ok, should not replace
                bool ok = false;
                while (is_shell_var_char(src.current())) {
                    o.push_back(src.current());
                    src.consume();
                    ok = true;
                }
                return ok;
            };
            expand(out, read);
            if (!called) {
                read(helper::nop);
            }
            return res;
        }

        template <class T>
        constexpr bool expand(auto&& out, Sequencer<T>& src, auto&& expand_var, bool allow_invalid_var = true) {
            while (!src.eos()) {
                if (src.seek_if("$")) {
                    auto ptr = src.rptr;
                    if (!read_varchar_unix(out, src, expand_var) &&
                        !allow_invalid_var) {
                        return false;
                    }
                    // not consumed any, remain $
                    if (ptr == src.rptr) {
                        out.push_back('$');
                    }
                    continue;
                }
                out.push_back(src.current());
                src.consume();
            }
            return true;
        }

        template <class Input>
        constexpr bool expand(auto&& out, Input&& src, auto&& expand_var, bool allow_invalid_var = true) {
            auto seq = make_ref_seq(src);
            return expand(out, seq, expand_var, allow_invalid_var);
        }

        template <class Out, class Input>
        constexpr Out expand(Input&& src, auto&& expand_var, bool allow_invalid_var = true) {
            Out out;
            expand(out, src, expand_var, allow_invalid_var);
            return out;
        }

        constexpr auto expand_null() noexcept {
            return [](auto&& out, auto&& read) {};
        }

        template <class BufType>
        constexpr auto expand_varname() noexcept {
            return [](auto&& out, auto&& read) {
                BufType buf;
                if (!read(buf)) {
                    return;  // error
                }
                strutil::append(out, buf);
            };
        }

        namespace test {

            struct Buf : binary::writer {
                byte data[100];
                constexpr Buf()
                    : binary::writer(data) {}
                constexpr byte operator[](size_t i) const {
                    return written()[i];
                }

                constexpr size_t size() const {
                    return written().size();
                }
            };

            constexpr bool check_expand() {
                byte data[100];
                binary::writer w{data};
                auto test = make_ref_seq("$OBJECT + ${IDENTIFIER} + ${} + ${ANTI");
                expand(w, test, expand_null());
                auto eq = w.written();
                if (!strutil::equal(eq, " +  +  + ANTI")) {
                    return false;
                }
                w.reset();
                test.rptr = 0;
                expand(w, test, expand_varname<Buf>());
                eq = w.written();
                if (!strutil::equal(eq, "OBJECT + IDENTIFIER +  + ANTI")) {
                    return false;
                }
                return true;
            }

            static_assert(check_expand());
        }  // namespace test

    }  // namespace env
}  // namespace utils
