/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// bnf - bnf dialect parser
#pragma once
#include "../minilang/old/parser/minl_binary.h"
#include "../minilang/old/parser/minl_primitive.h"
#include "../minilang/old/parser/minl_block.h"
#include "../minilang/old/parser/minl_bnf.h"
#include "../minilang/old/minlutil.h"
#include "../minilang/old/parser/minl_comment.h"
#include "../utf/convert.h"
#include "../view/char.h"

namespace utils {
    namespace bnf {
        namespace p = minilang::parser;
        namespace minl = minilang;

        struct AnnonRange {
            std::shared_ptr<minl::RangeNode> node;
            size_t cache_min_ = 0;
            size_t cache_max_ = 0;
            bool enable = false;
        };

        struct Annotation {
            bool repeat = false;
            bool optional = false;
            bool enfatal = false;
            AnnonRange range;
        };

        enum class BnfType {
            literal,
            char_code,
            char_range,

            or_,
            rule_ref,
            group,
        };

        struct BnfDef;

        struct BnfTree {
            BnfType type = BnfType::literal;
            Annotation annon;
            std::shared_ptr<minl::MinNode> node;
            std::shared_ptr<BnfTree> child;
            std::shared_ptr<BnfTree> els;
            std::shared_ptr<BnfTree> next;

            std::weak_ptr<BnfDef> ref;
            std::string fmtcache;
            struct {
                size_t min_ = 0;
                size_t max_ = 0;
                bool enable = false;
                bool utf = false;
            } rangecache;
        };

        struct BnfDef {
            std::shared_ptr<minl::BinaryNode> node;
            std::shared_ptr<BnfTree> bnf;

            auto rule_name() {
                return node && node->left ? node->left->str : "";
            }
        };

        namespace internal {

            auto parser_ctx(auto&& errc, auto&& seq) {
                auto prim = p::or_(p::bnf_charcode(), p::string(false), p::ident());
                auto prims = p::or_(p::parenthesis(p::call_expr(), p::Br{"()", "(", ")"}, p::Br{"[]", "[", "]"}), p::call_prim());
                auto afters = p::afters(prims, p::or_(p::unary_after(p::expecter(true, p::Op{"?"}, p::Op{"*"}, p::Op{"!"}))));
                auto unary = p::bnf_range_prefix(afters, p::RangePrefixMode::allow);
                auto rep = p::repeat(true, false, p::eol_factor(unary));
                auto or_ = p::binary(rep, p::expecter(true, p::Op{"|"}));
                auto def_ = p::asymmetric("define", p::ident_or_err(), or_, p::expecter(false, p::Op{":="}));
                auto com = p::comment(p::ComBr{"#"});
                return p::make_ctx(or_, p::or_(com, def_), p::always_error(), prim, errc, seq);
            }

            auto parse(auto&& errc, auto&& seq) {
                auto p = parser_ctx(errc, seq);
                return p::repeat(true, true, p::call_stat())(p);
            }

            std::shared_ptr<BnfTree> walk(auto&& errc, std::shared_ptr<minilang::MinNode>& node, Annotation annon = {});

            inline std::shared_ptr<BnfTree> seq(auto&& errc, std::shared_ptr<minl::BlockNode>& block) {
                auto cur = block->next;
                std::shared_ptr<BnfTree> root, cur_bnf;
                while (cur) {
                    auto res = walk(errc, cur->element, {});
                    if (!res) {
                        return nullptr;
                    }
                    if (!cur_bnf) {
                        root = res;
                        cur_bnf = res;
                    }
                    else {
                        cur_bnf->next = res;
                        cur_bnf = std::move(res);
                    }
                    cur = cur->next;
                }
                return root;
            }

            inline std::shared_ptr<BnfTree> walk(auto&& errc, std::shared_ptr<minilang::MinNode>& node, Annotation annon) {
                if (auto bin = minl::is_Binary(node, true)) {
                    if (bin->str == "?") {
                        if (annon.optional) {
                            errc.say("already annotated with optional(symbol:?) or range prefix (symbol:<min>~<max>)");
                            errc.node(bin);
                            return nullptr;
                        }
                        annon.optional = true;
                        return walk(errc, bin->right, annon);
                    }
                    if (bin->str == "*") {
                        if (annon.repeat) {
                            errc.say("already annotated with repeat(symbol:*) or range prefix (symbol:<min>~<max>)");
                            errc.node(bin);
                            return nullptr;
                        }
                        annon.repeat = true;
                        return walk(errc, bin->right, annon);
                    }
                    if (bin->str == "!") {
                        if (annon.enfatal) {
                            errc.say("already annotated with fatal(symbol:!)");
                            errc.node(bin);
                            return nullptr;
                        }
                        annon.enfatal = true;
                        return walk(errc, bin->right, annon);
                    }
                    if (bin->str == "~") {
                        if (annon.repeat || annon.optional) {
                            errc.say("already annotated with repeat(symbol:*) or optional(symbol:?) or range prefix (symbol:<min>~<max>)");
                            errc.node(bin);
                            return nullptr;
                        }
                        auto range = minl::is_Range(bin->left);
                        if (!range) {
                            errc.say("expect range node but not");
                            errc.node(bin->left);
                            return nullptr;
                        }
                        annon.range.node = range;
                        annon.optional = true;
                        annon.repeat = true;
                        return walk(errc, bin->right, annon);
                    }
                    if (bin->str == "|") {
                        auto then = walk(errc, bin->left, {});
                        if (!then) {
                            return nullptr;
                        }
                        auto els = walk(errc, bin->right, {});
                        if (!els) {
                            return nullptr;
                        }
                        auto or_ = std::make_shared<BnfTree>();
                        or_->annon = annon;
                        or_->child = std::move(then);
                        or_->els = std::move(els);
                        or_->node = bin;
                        or_->type = BnfType::or_;
                        return or_;
                    }
                    if (bin->str == "()" || bin->str == "[]") {
                        auto inner = walk(errc, bin->right, {});
                        if (!inner) {
                            return nullptr;
                        }
                        auto tree = std::make_shared<BnfTree>();
                        if (bin->str == "[]") {
                            annon.optional = true;
                        }
                        tree->annon = annon;
                        tree->child = std::move(inner);
                        tree->node = bin;
                        tree->type = BnfType::group;
                        return tree;
                    }
                }
                if (minl::is_String(node)) {
                    auto lit = std::make_shared<BnfTree>();
                    lit->annon = annon;
                    lit->node = node;
                    lit->type = BnfType::literal;
                    return lit;
                }
                if (minl::is_Number(node)) {
                    auto code = std::make_shared<BnfTree>();
                    code->annon = annon;
                    code->node = node;
                    code->type = BnfType::char_code;
                    return code;
                }
                if (minl::is_Range(node)) {
                    auto range = std::make_shared<BnfTree>();
                    range->annon = annon;
                    range->node = node;
                    range->type = BnfType::char_range;
                    return range;
                }
                if (minl::is_Ident(node)) {
                    auto ref = std::make_shared<BnfTree>();
                    ref->annon = annon;
                    ref->node = node;
                    ref->type = BnfType::rule_ref;
                    return ref;
                }
                auto s = minl::is_Block(node, true);
                if (!s) {
                    errc.say("expect block node but not");
                    errc.node(node);
                    return nullptr;
                }
                return seq(errc, s);
            }
            inline std::shared_ptr<BnfDef> make_bnfdef(auto&& errc, std::shared_ptr<minl::MinNode>& node) {
                auto bin = minl::is_Binary(node, true);
                if (!bin || bin->str != ":=") {
                    return nullptr;
                }
                if (!minl::is_Ident(bin->left)) {
                    return nullptr;
                }
                auto bnf = walk(errc, bin->right);
                if (!bnf) {
                    return nullptr;
                }
                auto def = std::make_shared<BnfDef>();
                def->node = bin;
                def->bnf = std::move(bnf);
                return def;
            }
        }  // namespace internal

        // parse parses BNF dialect
        // this dialect is not ABNF
        // # comment
        // A:=B # definition and rule reference
        // B:="lit" # literal
        // C:= %x82-84 # char code
        // D:= C* # repeat (least one)
        // E:= C? # optional
        // F:= C?* # repeat (least zero)
        // G:= (D E F) # group
        // H:= D|E|F # choice
        // I:= [D E F] # optional group = (D E F)?
        // J:= 2~3D # ranged repeat (min~max)
        // K:= 4E # fixed repeat
        // L:= ~4F # maximum repeat (0~max)
        bool parse(auto&& seq, auto&& errc, auto&& add_def) {
            std::shared_ptr<minl::BlockNode> block = internal::parse(errc, seq);
            if (!block) {
                return false;
            }
            auto cur = block->next;
            while (cur) {
                auto elm = cur->element;
                if (minl::is_Comment(elm)) {
                    cur = cur->next;
                    continue;
                }
                auto def = internal::make_bnfdef(errc, elm);
                if (!def) {
                    return false;
                }
                if (!add_def(std::move(def))) {
                    return false;
                }
                cur = cur->next;
            }
            return true;
        }

        // resolve resolves rule references and validates range of numbers and literal escape string
        // this function also generates cache for literal and char_code
        bool resolve(const std::shared_ptr<BnfTree>& target, auto&& errc, auto&& get_ref) {
            if (!target) {
                errc.say("unexpected nullptr");
                return false;
            }
            auto cur = target;
            while (cur) {
                if (cur->annon.range.node) {
                    auto range = cur->annon.range.node;
                    size_t min_ = 0, max_ = ~0;
                    if (range->max_) {
                        if (!range->max_->parsable) {
                            errc.say("too big integer");
                            errc.node(range->max_);
                            return false;
                        }
                        max_ = range->max_->integer;
                    }
                    if (range->min_) {
                        if (!range->min_->parsable) {
                            errc.say("too big integer");
                            errc.node(range->min_);
                            return false;
                        }
                        min_ = range->min_->integer;
                    }
                    if (min_ > max_) {
                        errc.say("invalid range");
                        errc.node(range);
                        return false;
                    }
                    cur->annon.range.cache_max_ = max_;
                    cur->annon.range.cache_min_ = min_;
                    cur->annon.range.enable = true;
                }
                if (cur->type == BnfType::or_) {
                    if (!resolve(cur->child, errc, get_ref) || !resolve(cur->els, errc, get_ref)) {
                        return false;
                    }
                }
                else if (cur->type == BnfType::group) {
                    if (!resolve(cur->child, errc, get_ref)) {
                        return false;
                    }
                }
                else if (cur->type == BnfType::literal) {
                    auto seq = make_ref_seq(cur->node->str);
                    std::string unescaped;
                    if (!escape::read_string(unescaped, seq, escape::ReadFlag::escape)) {
                        errc.say("failed to unescape string");
                        errc.node(cur->node);
                        return false;
                    }
                    cur->fmtcache = std::move(unescaped);
                }
                else if (cur->type == BnfType::char_code) {
                    auto num = minl::is_Char(cur->node);
                    if (!num) {
                        errc.say("expect char node but not");
                        errc.node(cur->node);
                        return false;
                    }
                    if (num->is_utf) {
                        if (!num->parsable || num->integer >= 0x110000) {
                            errc.say("too big integer for utf char code");
                            errc.node(num);
                            return false;
                        }
                        std::string utf8;
                        if (!utf::convert(view::CharView(std::uint32_t(num->integer)), utf8)) {
                            errc.say("failed to convert char code to utf8");
                            errc.node(num);
                            return false;
                        }
                        cur->fmtcache = std::move(utf8);
                    }
                    else {
                        if (!num->parsable || num->integer > 0xff) {
                            errc.say("too big integer for byte char code");
                            errc.node(num);
                            return false;
                        }
                        cur->fmtcache = std::string(1, char(num->integer));
                    }
                }
                else if (cur->type == BnfType::char_range) {
                    auto range = minl::is_Range(cur->node);
                    if (!range) {
                        errc.say("expect range node but not");
                        errc.node(cur->node);
                        return false;
                    }
                    auto mxc = minl::is_Char(range->max_);
                    if (!mxc) {
                        errc.say("expect char node but not");
                        errc.node(range->max_);
                        return false;
                    }
                    if (mxc->is_utf) {
                        if (!range->max_->parsable || range->max_->integer >= 0x110000) {
                            errc.say("too big integer for utf char code");
                            errc.node(range->max_);
                            return false;
                        }
                        if (!range->min_->parsable || range->min_->integer >= 0x110000) {
                            errc.say("too big integer for utf char code");
                            errc.node(range->min_);
                            return false;
                        }
                    }
                    else {
                        if (!range->max_->parsable || range->max_->integer > 0xff) {
                            errc.say("too big integer for byte char code");
                            errc.node(range->max_);
                            return false;
                        }
                        if (!range->min_->parsable || range->min_->integer >= 0xff) {
                            errc.say("too big integer for byte char code");
                            errc.node(range->min_);
                            return false;
                        }
                    }
                    if (range->min_->integer > range->max_->integer) {
                        errc.say("invalid char code range");
                        errc.node(range);
                        return false;
                    }
                    cur->rangecache.min_ = range->min_->integer;
                    cur->rangecache.max_ = range->max_->integer;
                    cur->rangecache.utf = mxc->is_utf;
                    cur->rangecache.enable = true;
                }
                else if (cur->type == BnfType::rule_ref) {
                    std::shared_ptr<BnfDef> ref = get_ref(cur->node->str);
                    if (!ref) {
                        if (cur->node->str != "EOF") {
                            errc.say("reference to ", cur->node->str, " is not found");
                            errc.node(cur->node);
                            return false;
                        }
                    }
                    cur->ref = ref;
                }
                cur = cur->next;
            }
            return true;
        }

    }  // namespace bnf
}  // namespace utils
