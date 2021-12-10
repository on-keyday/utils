/*license*/

// tokenizer - custom tokenizer for syntax
#pragma once
#include "../../tokenize/tokenizer.h"
#include "../../tokenize/merge.h"
#include "keyword.h"
#include "attribute.h"

namespace utils {
    namespace syntax {
        namespace tknz = tokenize;

        template <class String, template <class...> class Vec = wrap::vector>
        tknz::Tokenizer<String, Vec> make_tokenizer() {
            tknz::Tokenizer<String, Vec> ret;
            auto cvt_push = [&](auto base, auto& predef) {
                String converted;
                utf::convert(base, converted);
                predef.push_back(std::move(converted));
            };
            for (auto i = 0; i < sizeof(keyword_str) / sizeof(keyword_str[0]); i++) {
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
            auto tokenizer = make_tokenizer<String, Vec>();
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
    }  // namespace syntax
}  // namespace utils
