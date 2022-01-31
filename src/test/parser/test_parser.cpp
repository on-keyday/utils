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
#include <parser/complex_parser.h>
#include <parser/parser_compiler.h>
#include <helper/line_pos.h>
enum class TokKind {
    line,
    space,
    blanks,
    keyword,
    symbol,
    segment,
    identifier,
    string,
};
BEGIN_ENUM_STRING_MSG(TokKind, to_string)
ENUM_STRING_MSG(TokKind::line, "line")
ENUM_STRING_MSG(TokKind::space, "space")
ENUM_STRING_MSG(TokKind::blanks, "blanks")
ENUM_STRING_MSG(TokKind::identifier, "identifier")
ENUM_STRING_MSG(TokKind::segment, "segment")
ENUM_STRING_MSG(TokKind::keyword, "keyword")
ENUM_STRING_MSG(TokKind::symbol, "symbol")
ENUM_STRING_MSG(TokKind::string, "string")
END_ENUM_STRING_MSG("unknown")

template <class Input>
using parser_t = utils::wrap::shared_ptr<utils::parser::Parser<Input, utils::wrap::string, TokKind, utils::wrap::vector>>;
using token_t = utils::wrap::shared_ptr<utils::parser::Token<utils::wrap::string, TokKind, utils::wrap::vector>>;

template <class Input>
parser_t<Input> make_parser(utils::Sequencer<Input>& seq) {
    using namespace utils::wrap;
    auto space = utils::parser::blank_parser<Input, string, TokKind, vector>(TokKind::space, TokKind::line, TokKind::blanks, "blanks");
    auto struct_ = utils::parser::make_tokparser<Input, string, TokKind, vector>("struct", TokKind::keyword);
    auto begin_br = utils::parser::make_tokparser<Input, string, TokKind, vector>("{", TokKind::symbol);
    auto end_br = utils::parser::make_tokparser<Input, string, TokKind, vector>("}", TokKind::symbol);
    auto some_space = utils::parser::make_allownone(parser_t<Input>{space});
    auto identifier = utils::parser::make_anyother<Input, string, TokKind, vector>(vector<string>{"struct"}, vector<string>{"{", "}", "\""}, TokKind::identifier);
    auto struct_group = utils::parser::make_and<Input, string, TokKind, vector>(vector<parser_t<Input>>{
                                                                                    some_space,
                                                                                    struct_,
                                                                                    space,
                                                                                    identifier,
                                                                                    some_space,
                                                                                    begin_br,
                                                                                    some_space,
                                                                                    end_br,
                                                                                },
                                                                                "struct", TokKind::segment);
    auto str = utils::parser::string_parser<Input, string, TokKind, vector>(TokKind::string, TokKind::symbol, TokKind::segment, "string", "\"", "\\");
    auto seq2 = utils::make_ref_seq(R"(
    config:={
        "ignore": "blank"
    }
    string_quote:="\""
    DO:="1" "+" "1"
    ROOT:=DO STRUCT ID STRUCT
    STRUCT:="struct" ID BLANK? "{" BLANK? "}" 
)");
    auto res = utils::parser::compile_parser<Input, string, TokKind, vector>(seq2, [](auto& tok, utils::parser::KindMap kind) {
        using namespace utils::parser;
        if (kind == KindMap::space || kind == KindMap::blank) {
            return TokKind::space;
        }
        return TokKind::symbol;
    });
    assert(res);
    return res;
    //return utils::parser::make_and<Input, string, TokKind, vector>(vector<parser_t<Input>>{struct_group, some_space, str}, "struct and str", TokKind::segment);
}

template <class Json>
void to_json(const token_t& tok, Json& json) {
    if (!tok || tok->kind == TokKind::space) {
        return;
    }
    Json js;
    js["token"] = tok->token;
    js["kind"] = to_string(tok->kind);
    auto& pos = js["pos"];
    pos["line"] = tok->pos.line;
    pos["pos"] = tok->pos.pos;
    pos["rptr"] = tok->pos.rptr;
    for (auto i = 0; i < tok->child.size(); i++) {
        to_json(tok->child[i], js["child"]);
    }
    json.push_back(std::move(js));
}

int main() {
    auto seq = utils::make_ref_seq(R"(1 + 1  struct Hello{}
     structs   struct ID{}
)");
    auto parser = make_parser(seq);
    utils::parser::Pos pos;
    auto res = parser->parse(seq, pos);
    utils::json::OrderedJSON js;
    to_json(res.tok, js);
    utils::wrap::string str;
    utils::helper::write_src_loc(str, seq);
    utils::wrap::cout_wrap() << res.err.Error() << "\n"
                             << str << "\n";
    utils::wrap::cout_wrap() << utils::json::to_string<utils::wrap::string>(js);
}
