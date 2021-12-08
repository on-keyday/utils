/*license*/

// parse - public parse interface
#pragma once
#include "parse_impl.h"

namespace utils {
    namespace syntax {
        template <class String, template <class...> class Vec, template <class...> class Map>
        bool parse(internal::ParseContext<String>& ctx, Map<String, wrap::shared_ptr<Element<String, Vec>>>& result) {
            while (ctx.r.read()) {
                String segname;
                wrap::shared_ptr<Element<String, Vec>> group;
                if (!internal::parse_a_line(ctx, segname, group)) {
                    if (ctx.err.empty()) {
                        ctx.err.packln("error: unhandled error occured");
                    }
                    return false;
                }
                auto res = result.insert({segname, group});
                if (!std::get<1>(res)) {
                    ctx.err.packln("error: element `", segname, "` is already exists");
                    return false;
                }
            }
            return true;
        }

        template <class String>
        internal::ParseContext<String> make_parse_context(wrap::shared_ptr<tknz::Token<String>> p) {
            return internal::ParseContext<String>{
                Reader<String>(p),
            };
        }
    }  // namespace syntax
}  // namespace utils
