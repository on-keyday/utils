/*license*/

// predefined - predefined keyword, context keyword, and symbol
#pragma once

#include "../core/sequencer.h"

#include "space.h"
#include "line.h"

namespace utils {
    namespace tokenize {
        template <class String, template <class...> class Vec>
        struct Predefined {
            Vec<String> predef;
            template <class T>
            size_t match(Sequencer<T>& seq, String& matched) const {
                for (auto& v : predef) {
                    if (auto n = seq.match_n(v)) {
                        matched = v;
                        return n;
                    }
                }
            }
        };

        template <class T, class String>
        constexpr bool or_match(Sequencer<T>& seq, String& matched) {
            return false;
        }

        template <class T, class String, class Predef, class... Args>
        constexpr bool or_match(Sequencer<T>& seq, String& matched, Predef&& t, Args&&... args) {
            return t.match() || or_match(seq, matched, std::forward<Args>(args)...);
        }

        template <class T, class String, template <class...> class Vec, class... Args>
        constexpr bool match(Sequencer<T>& seq, Predefined<String, Vec>& predef, String& matched, Args&&... args) {
            if (predef.match(seq, matched)) {
                match_line(seq, );
            }
        }
    }  // namespace tokenize
}  // namespace utils
