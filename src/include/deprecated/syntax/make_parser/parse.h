/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse - public parse interface
#pragma once
#include "parse_impl.h"

#include <wrap/light/vector.h>
#include <algorithm>

namespace utils {
    namespace syntax {
        template <class String, template <class...> class Vec, template <class...> class Map>
        bool parse(internal::ParseContext<String, Vec>& ctx, Map<String, wrap::shared_ptr<Element<String, Vec>>>& result) {
            auto arr = {"\"", "+", "-", ".", "\\"};
            for (auto a : arr) {
                String tmp;
                utf::convert(a, tmp);
                ctx.symbol.push_back(std::move(tmp));
            }
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
                if (!get<1>(res)) {
                    ctx.err.packln("error: element `", segname, "` is already exists");
                    return false;
                }
            }
            auto sorter = [](const std::string& a, const std::string& b) {
                return a.size() > b.size();
            };
            std::sort(ctx.keyword.begin(), ctx.keyword.end(), sorter);
            std::sort(ctx.symbol.begin(), ctx.symbol.end(), sorter);
            return true;
        }

        template <class String, template <class...> class Vec = wrap::vector>
        internal::ParseContext<String, Vec> make_parse_context(wrap::shared_ptr<tknz::Token<String>> p) {
            return internal::ParseContext<String, Vec>{
                Reader<String>(p),
            };
        }
    }  // namespace syntax
}  // namespace utils
