/*license*/

// ref_or - matching `or` element and `reference` element
#pragma once
#include "context.h"
#include "state.h"

namespace utils {
    namespace syntax {
        template <class String, template <class...> class Vec>
        MatchState match_or(Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v) {
            auto& r = ctx.r;
        }
    }  // namespace syntax
}  // namespace utils
