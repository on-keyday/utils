/*license*/

// parse - option parse
#pragma once

#include "cast.h"
#include "option_result.h"

namespace utils {
    namespace cmdline {
        enum class ParseError {
            none,
            not_one_opt,
        };

        struct OptionParser {
            template <class Char, class OptName, class String, template <class...> class Vec, template <class...> class MultiMap>
            ParseError parse_one(int& index, int argc, Char** argv, const OptName& name,
                                 Option<String, Vec>& option,
                                 OptionResultSet<String, Vec, MultiMap>& result) {
                if (any(option.attr & OptionAttribute::once_in_cmd)) {
                    if (result.exists(name)) {
                        return ParseError::not_one_opt;
                    }
                }
            }
        };
    }  // namespace cmdline
}  // namespace utils
