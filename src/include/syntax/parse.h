/*license*/

// parse - parse method
#pragma once

#include "element.h"

namespace utils {
    namespace syntax {

        template <class String, template <class...> class Vec>
        bool parse_group(Reader<String>& r, wrap::shared_ptr<Element<String, Vec>>& group) {
        }
    }  // namespace syntax
}  // namespace utils