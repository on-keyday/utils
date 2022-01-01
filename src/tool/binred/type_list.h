
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
        utw::string val;
        FlagType type = FlagType::none;
        utils::syntax::KeyWord kind = {};
    };

    struct Type {
        utw::string name;
        Flag flag;
    };

    struct Member {
        utw::string name;
        Type type;
        utw::string defval;
        utils::syntax::KeyWord kind = {};
    };

    struct Struct {
        utw::vector<Member> member;
    };

    struct FileData {
        utw::map<utw::string, Struct> structs;
        utw::vector<utw::string> defvec;
    };

    struct State {
        FileData data;
        utw::string cuurent_struct;
    };

    enum class GenFlag {

    };

    bool read_fmt(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state);
    void generate_cpp(utw::string& str, FileData& data, GenFlag flag);

}  // namespace binred
