
// binred - binary reader helper
#pragma once

#include "../../include/wrap/lite/lite.h"
#include "../../include/syntax/make_parser/keyword.h"
#include "../../include/syntax/matching/matching.h"

namespace binred {
    namespace utw = utils::wrap;
    enum class FlagType {
        none,
        eq,
        bit,
    };

    struct Flag {
        utw::string depend;
        size_t num = 0;
        FlagType type = FlagType::none;
    };

    struct Type {
        utw::string name;
        Flag flag;
    };

    struct Member {
        utw::string name;
        Type type;
        utw::string defval;
    };

    struct Struct {
        utw::vector<Member> member;
    };

    struct FileData {
        utw::map<utw::string, Struct> structs;
        utw::vector<utw::string> defvec;
    };

    struct State {
    };

}  // namespace binred
