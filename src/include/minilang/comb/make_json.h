/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "branch_table.h"
#include "../../number/to_string.h"
#include "../../escape/escape.h"

namespace utils::minilang::comb::branch {

    template <class GroupTag = const char*, class IdentTag = const char*>
    auto make_json(auto& out) {
        auto escape = [](auto&& in) {
            std::string str;
            escape::escape_str(in, str, escape::EscapeFlag::utf | escape::EscapeFlag::hex);
            return "\"" + str + "\"";
        };
        return [&, escape](const std::shared_ptr<Element>& elm, CallEntry ent) {
            if (auto el = is_Branch(elm); el && el->status == CombStatus::not_match) {
                return CallNext::skip;
            }
            if (auto el = is_Group<GroupTag>(elm); el) {
                if (ent == CallEntry::enter) {
                    out += "{" + escape(convert_tag_to_string(el->tag)) + ":[";
                }
                else {
                    out.pop_back();
                    out += "]},";
                }
                return CallNext::cont;
            }
            if (ent == CallEntry::leave) {
                return CallNext::skip;
            }
            auto add_pos = [&](Pos pos) {
                out += "\"pos\":{\"begin\":" +
                       utils::number::to_string<std::string>(pos.begin) +
                       ",\"end\":" +
                       utils::number::to_string<std::string>(pos.end) +
                       "}";
            };
            if (auto tok = is_Token(elm)) {
                out += "{\"token\":" + escape(tok->token) + ",";
                add_pos(tok->pos);
                out += "},";
            }
            else if (auto id = is_Ident<IdentTag>(elm)) {
                out += "{" + escape(convert_tag_to_string(id->tag)) + ":" + escape(id->ident) + ",";
                add_pos(id->pos);
                out += "},";
            }
            return CallNext::cont;
        };
    }
}  // namespace utils::minilang::comb::branch
