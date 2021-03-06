/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// predefined - predefined keyword, context keyword, and symbol
#pragma once

#include "../core/sequencer.h"

#include "space.h"
#include "line.h"

namespace utils {
    namespace tokenize {
        template <class String, template <class...> class Vec>
        struct Predefined {
            Vec<String> predef;
            template <class T>
            size_t match(Sequencer<T>& seq, String& matched) const {
                for (auto& v : predef) {
                    if (auto n = seq.match_n(v)) {
                        matched = v;
                        return n;
                    }
                }
                return 0;
            }
        };

        template <class String, template <class...> class Vec>
        struct PredefinedCtx {
            Vec<std::pair<String, size_t>> predef;

            template <class T>
            size_t match(Sequencer<T>& seq, String& matched, size_t* layer = nullptr) const {
                for (auto& v : predef) {
                    if (auto n = seq.match_n(get<0>(v))) {
                        matched = get<0>(v);
                        if (layer) {
                            *layer = get<1>(v);
                        }
                        return n;
                    }
                }
                return 0;
            }
        };

        namespace internal {

            template <class T, class String>
            constexpr bool or_match(Sequencer<T>& seq, String& matched) {
                return false;
            }

            template <class T, class String, class Predef, class... Args>
            constexpr bool or_match(Sequencer<T>& seq, String& matched, Predef&& t, Args&&... args) {
                return t.match(seq, matched) || or_match(seq, matched, std::forward<Args>(args)...);
            }

            template <class String, class T, class... Args>
            constexpr bool is_not_separated(Sequencer<T>& seq, Args&&... args) {
                String tmp;
                return match_line(seq, nullptr) == LineKind::unknown && !helper::space::match_space(seq) && !or_match(seq, tmp, std::forward<Args>(args)...);
            }
        }  // namespace internal

        template <bool check_after, class T, class String, template <class...> class Vec, class... Args>
        constexpr bool read_predefined(Sequencer<T>& seq, Predefined<String, Vec>& predef, String& matched, Args&&... args) {
            if (auto matchsize = predef.match(seq, matched)) {
                seq.consume(matchsize);
                if (check_after) {
                    if (internal::is_not_separated<String>(seq, std::forward<Args>(args)...)) {
                        seq.backto(matchsize);
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        template <bool check_after, class T, class String, template <class...> class Vec, class... Args>
        constexpr bool read_predefined_ctx(Sequencer<T>& seq, PredefinedCtx<String, Vec>& predef, String& matched, size_t& layer, Args&&... args) {
            if (auto matchsize = predef.match(seq, matched, &layer)) {
                seq.consume(matchsize);
                if (check_after) {
                    if (internal::is_not_separated<String>(seq, std::forward<Args>(args)...)) {
                        seq.backto(matchsize);
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        template <class String>
        struct Predef : Token<String> {
            String token;

            constexpr Predef(TokenKind kind)
                : Token<String>(kind) {}

            String to_string() const override {
                return token;
            }

            bool has(const String& str) const override {
                return token == str;
            }
        };

        template <class String>
        struct PredefCtx : Token<String> {
            String token;
            size_t layer_ = 0;

            constexpr PredefCtx()
                : Token<String>(TokenKind::context) {}

            String to_string() const override {
                return token;
            }

            bool has(const String& str) const override {
                return token == str;
            }

            size_t layer() const override {
                return layer_;
            }
        };
    }  // namespace tokenize
}  // namespace utils
