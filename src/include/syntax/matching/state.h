/*license*/

// state - matching state

namespace utils {
    namespace syntax {
        enum class MatchState {
            succeed,
            not_match,
            eof,
            fatal,
        };
    }  // namespace syntax
}  // namespace utils