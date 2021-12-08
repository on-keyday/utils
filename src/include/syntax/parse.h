/*license*/

// parse - public parse interface

#include "parse_impl.h"

namespace utils {
    namespace syntax {
        template <class String, template <class...> class Vec>
        bool parse(Reader<String>& r) {
            internal::ParseContext<String> ctx;
            ctx.r = r.from_current();
        }
    }  // namespace syntax
}  // namespace utils
