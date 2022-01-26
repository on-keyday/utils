/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <parser/logical_parser.h>
#include <parser/space_parser.h>
#include <parser/token_parser.h>
#include <wrap/lite/lite.h>
#include <json/json_export.h>
#include <json/convert_json.h>
#include <wrap/cout.h>
enum class TokKind {
    line,
    space,
    blanks,
    keyword,
    symbol,
    segment,
};
BEGIN_ENUM_STRING_MSG(TokKind, to_string)
ENUM_STRING_MSG(TokKind::line, "line")
ENUM_STRING_MSG(TokKind::space, "space")
ENUM_STRING_MSG(TokKind::blanks, "blanks")
END_ENUM_STRING_MSG("unknown")

template <class Input>
using parser_t = utils::wrap::shared_ptr<utils::parser::Parser<Input, utils::wrap::string, TokKind, utils::wrap::vector>>;
using token_t = utils::wrap::shared_ptr<utils::parser::Token<utils::wrap::string, TokKind, utils::wrap::vector>>;

template <class Input>
parser_t<Input> make_parser(utils::Sequencer<Input>& seq) {
    using namespace utils::wrap;
    auto line = utils::parser::make_line<Input, string, TokKind, vector>(TokKind::line);
    auto space = utils::parser::make_spaces<Input, string, TokKind, vector>(TokKind::space);
    auto or_ = utils::parser::make_or<Input, string, TokKind, vector>(vector<parser_t<Input>>{line, space});
    auto repeat = utils::parser::make_repeat(parser_t<Input>(or_), "blanks", TokKind::blanks);
    auto struct_ = utils::parser::make_tokparser<Input, string, TokKind, vector>("struct", TokKind::keyword);
    auto begin_br = utils::parser::make_tokparser<Input, string, TokKind, vector>("{", TokKind::symbol);
    auto end_br = utils::parser::make_tokparser<Input, string, TokKind, vector>("}", TokKind::symbol);
    auto some_space = utils::parser::make_allownone(parser_t<Input>{space});
    auto struct_group = utils::parser::make_and<Input, string, TokKind, vector>(vector<parser_t<Input>>{
                                                                                    struct_,
                                                                                    space,
                                                                                    some_space,
                                                                                    begin_br,
                                                                                    some_space,
                                                                                    end_br,
                                                                                },
                                                                                "struct", TokKind::segment);
    return struct_group;
}

template <class Json>
void to_json(const token_t& tok, Json& json) {
    Json js;
    js["token"] = tok->token;
    js["kind"] = to_string(tok->kind);
    auto& pos = js["pos"];
    pos["line"] = tok->pos.line;
    pos["pos"] = tok->pos.pos;
    for (auto i = 0; i < tok->child.size(); i++) {
        to_json(tok->child[i], pos["children"]);
    }
    json.push_back(std::move(js));
}

int main() {
    auto seq = utils::make_ref_seq("  \n\r\n       ");
    auto parser = make_parser(seq);
    utils::parser::Pos pos;
    auto res = parser->parse(seq, pos);
    utils::json::OrderedJSON js;
    to_json(res.tok, js);

    utils::wrap::cout_wrap() << utils::json::to_string<utils::wrap::string>(js);
}
