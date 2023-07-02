/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <vector>
#include <string>
#include <variant>
#include <json/json_export.h>
#include <code/code_writer.h>
#include <helper/defer.h>
#include <minilang/comb/nullctx.h>
#include <string_view>

namespace binp {

    using Pos = utils::minilang::comb::Pos;

    using Out = utils::code::CodeWriter<std::string, std::string_view>;

    struct Attributes {
        Pos default_pos;
        std::string default_value;
        Pos encode_pos;
        std::string encode_method;
        Pos decode_pos;
        std::string decode_method;
    };

    struct Field;

    struct Struct {
        Pos struct_pos;
        Pos begin_pos;
        std::vector<Field> fields;
        Pos end_pos;
    };

    struct IdentTy {
        Pos name_pos;
        std::string name;
    };

    struct Type {
        std::variant<IdentTy, Struct> type;
        Attributes attrs;
    };

    struct Field {
        Pos name_pos;
        std::string name;
        Type type;
    };

    struct TypeDef {
        Pos type_pos;
        Pos name_pos;
        std::string name;
        Type base;
    };

    struct Program {
        std::string namespace_;
        std::vector<std::string> includes;
        std::vector<TypeDef> typedefs;
    };
    namespace json = utils::json;
    bool traverse(Program& scope, const json::JSON& node);

    struct Err {
       private:
        std::string err;
        Pos pos;

       public:
        Err() = default;
        Err(std::string&& err, Pos pos)
            : err(std::move(err)), pos(pos) {}
    };

    struct Errs {
       private:
        std::vector<Err> errs;

       public:
        void push_back(Err&& e) {
            errs.push_back(std::move(e));
        }
    };

    struct Env {
        Out out;
        Errs errs;
    };

    bool generate(Env& env, const Program& prog);

}  // namespace binp
