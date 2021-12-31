
// binred - binary reader helper
#pragma once

#include "../../include/wrap/lite/string.h"

namespace binred {
    namespace utw = utils::wrap;
    struct Flag {
        utw::string depend;
        size_t num;
    };

    struct Type {
        utw::string name;
    };
}  // namespace binred