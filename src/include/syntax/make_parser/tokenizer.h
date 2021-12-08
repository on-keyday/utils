/*license*/

#include "../../tokenize/tokenizer.h"
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
            cvt_push("[", ret.symbol.predef);
            cvt_push("]", ret.symbol.predef);
            cvt_push("|", ret.symbol.predef);
            cvt_push("#", ret.symbol.predef);
            cvt_push("\"", ret.symbol.predef);
            cvt_push(attribute(Attribute::adjacent), ret.symbol.predef);
            cvt_push(attribute(Attribute::fatal), ret.symbol.predef);
            cvt_push(attribute(Attribute::ifexists), ret.symbol.predef);
            cvt_push(attribute(Attribute::repeat), ret.symbol.predef);
            return ret;
        }
    }  // namespace syntax
}  // namespace utils