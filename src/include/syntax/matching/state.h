/*license*/

// state - matching state
#pragma once

namespace utils {
    namespace syntax {
        enum class MatchState {
            succeed,
            not_match,
            eof,
            fatal,
            no_repeat,
        };
    }  // namespace syntax
}  // namespace utils