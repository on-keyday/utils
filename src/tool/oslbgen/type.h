/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <variant>
#include <comb2/pos.h>
#include <string>
#include <vector>
#include <comb2/tree/simple_node.h>
#include <code/code_writer.h>

namespace oslbgen {
    using Pos = utils::comb2::Pos;

    struct Spec {
        Pos pos;
        std::string spec;
        std::vector<std::string> targets;
    };

    struct Fn {
        Pos pos;
        std::string fn_name;
        Spec spec;
        std::string args;
        std::string ret;
    };

    struct Define {
        Pos pos;
        std::string macro_name;
        bool is_fn_style = false;
        std::vector<std::string> args;
        std::string token;
    };

    struct Strc {
        Pos pos;
        std::string type;
        Spec spec;
        std::string name;
        std::string inner;
    };

    struct Use {
        Pos pos;
        std::string use;
    };

    struct Alias {
        Pos pos;
        std::string name;
        std::string token;
    };

    struct Preproc {
        Pos pos;
        std::string directive;
    };

    struct Comment {
        Pos pos;
        std::string comment;
    };
    constexpr auto cmt_index = 0;
    using Data = std::variant<Comment, Fn, Spec, Define, Strc, Use, Alias, Preproc>;

    struct DataSet {
        std::vector<Data> dataset;
        std::string ns;
    };

    namespace node = utils::comb2::tree::node;
    constexpr auto npos = utils::comb2::npos;
    Pos node_root(DataSet& db, std::shared_ptr<node::Node> tok);
    using Out = utils::code::CodeWriter<std::string>;

    Pos write_code(Out& out, DataSet& db);
}  // namespace oslbgen
