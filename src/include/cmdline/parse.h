/*license*/

// parse - option parse
#pragma once

#include "cast.h"
#include "option_result.h"
#include "../helper/strutil.h"

namespace utils {
    namespace cmdline {
        enum class ParseError {
            none,
            not_one_opt,
            not_assigned,
            bool_not_true_or_false,
        };
        /*
        template <class Char, class OptName, class String, template <class...> class Vec, template <class...> class MultiMap>
        struct ParseContext {
            int& index;
            int argc;
            Char** argv;
            const OptName& name;
            Option<String, Vec>& option;
            OptionResultSet<String, Vec, MultiMap>& result;
            String* assign = nullptr
        };*/

        namespace internal {
            template <class Char, class String, template <class...> class Vec>
            ParseError parse_booloption(BoolOption<String, Vec>* booopt, int& index, int argc, Char** argv, OptionResult<String, Vec>& result, String* assign) {
                auto v = wrap::make_shared<BoolOption<String, Vec>>();
                v->value = boopt->value;
                result.value = v;
                if (any(option.attr & OptionAttribute::must_assign)) {
                    if (!assign) {
                        return ParseError::none;
                    }
                }
                if (assign) {
                    if (helper::equal(*assign, "true")) {
                        v->value = true;
                    }
                    else if (helper::equal(*assign, "false")) {
                        v->value = false;
                    }
                    else {
                        return ParseError::bool_not_true_or_false;
                    }
                }
                else {
                    if (index + 1 < argc) {
                        if (helper::equal(argv[index + 1], "true")) {
                            v->value = true;
                            index++;
                        }
                        else if (helper::equal(argv[index + 1], "false")) {
                            v->value = false;
                            index++;
                        }
                    }
                }
                return ParseError::none;
            }
        }  // namespace internal
        template <class Char, class OptName, class String, template <class...> class Vec, template <class...> class MultiMap>
        ParseError parse_one(int& index, int argc, Char** argv, const OptName& name,
                             Option<String, Vec>& option,
                             OptionResultSet<String, Vec, MultiMap>& result, String* assign = nullptr) {
            if (any(option.attr & OptionAttribute::once_in_cmd)) {
                if (result.exists(name)) {
                    return ParseError::not_one_opt;
                }
            }
            OptionResult<String, Vec> optres;
            optres.base = std::addressof(option);
            if (BoolOption<String, Vec>* boopt = cast<BoolOption>(&option)) {
                return internal::parse_booloption(boopt, index, argc, argv, optres, assign);
            }
        }

        enum class ParseFlag {
            none,
            two_prefix_longname = 0x1,  // `--` is long name
            allow_assign = 0x2,         // allow `=` operator
            adjacent_value = 0x4,       // -oValue is allowed (default `-oValue` become o,V,a,l,u,e option)

        };

        DEFINE_ENUM_FLAGOP(ParseFlag)

    }  // namespace cmdline
}  // namespace utils
