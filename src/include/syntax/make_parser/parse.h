/*license*/

// parse - public parse interface

#include "parse_impl.h"

namespace utils {
    namespace syntax {
        template <class String, template <class...> class Vec, template <class...> class Map>
        bool parse(internal::ParseContext<String>& ctx, Map<String, wrap::shared_ptr<Element<String, Vec>>>& result) {
            while (ctx.r.read()) {
                String segname;
                wrap::shared_ptr<Element<String, Vec>> group;
                if (!internal::parse_a_line(ctx, segname, group)) {
                    return false;
                }
                auto res = result.insert({segname, group});
                if (!std::get<1>(res)) {
                    return false;
                }
            }
        }
    }  // namespace syntax
}  // namespace utils
