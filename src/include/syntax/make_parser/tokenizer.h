/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tokenizer - custom tokenizer for syntax
#pragma once
#include "../../tokenize/tokenizer.h"
#include "../../tokenize/merge.h"
#include "keyword.h"
#include "attribute.h"

namespace utils {
    namespace syntax {
        namespace tknz = tokenize;
        namespace internal {
            template <class String, template <class...> class Vec = wrap::vector>
            tknz::Tokenizer<String, Vec> make_internal_tokenizer() {
                tknz::Tokenizer<String, Vec> ret;
                auto cvt_push = [&](auto base, auto& predef) {
                    String converted;
                    utf::convert(base, converted);
                    predef.push_back(std::move(converted));
                };
                for (size_t i = 0; i < sizeof(keyword_str) / sizeof(keyword_str[0]); i++) {
                    cvt_push(keyword_str[i], ret.keyword.predef);
                }
                cvt_push(":=", ret.symbol.predef);
                cvt_push("[", ret.symbol.predef);
                cvt_push("]", ret.symbol.predef);
                cvt_push("|", ret.symbol.predef);
                cvt_push("#", ret.symbol.predef);
                cvt_push("\"", ret.symbol.predef);
                cvt_push("\\", ret.symbol.predef);
                cvt_push(attribute(Attribute::adjacent), ret.symbol.predef);
                cvt_push(attribute(Attribute::fatal), ret.symbol.predef);
                cvt_push(attribute(Attribute::ifexists), ret.symbol.predef);
                cvt_push(attribute(Attribute::repeat), ret.symbol.predef);
                return ret;
            }

            template <class T, class String, template <class...> class Vec = wrap::vector>
            bool tokenize_and_merge(Sequencer<T>& input, wrap::shared_ptr<tknz::Token<String>>& output, const char** errmsg = nullptr) {
                auto tokenizer = internal::make_internal_tokenizer<String, Vec>();
                auto result = tokenizer.tokenize(input, output);
                assert(result && "expect true but tokenize failed");
                const char* err = nullptr;
                auto res = tknz::merge(err, output, tknz::escaped_comment<String>("\"", "\\"),
                                       tknz::line_comment<String>("#"));
                if (errmsg) {
                    *errmsg = err;
                }
                return res;
            }
        }  // namespace internal

    }  // namespace syntax
}  // namespace utils
