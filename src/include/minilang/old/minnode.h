/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minnode - node types
#pragma once
#include <memory>
#include <string>
#include <number/prefix.h>
#include <helper/space.h>
#include <view/slice.h>

namespace utils {
    namespace minilang {
        struct Pos {
            size_t begin = 0;
            size_t end = 0;
        };

        constexpr auto invalid_pos = Pos{size_t(~0), size_t(~0)};

        enum NodeType {
            nt_min = 0x0000,
            nt_binary = 0x0001,
            nt_cond = 0x0101,
            nt_for = 0x1101,
            nt_if = 0x2101,
            nt_type = 0x0002,
            nt_arrtype = 0x0102,
            nt_gentype = 0x0202,
            nt_struct_field = 0x0302,
            nt_funcparam = 0x0402,
            nt_func = 0x1402,
            nt_interface = 0x0502,
            nt_typedef = 0x0003,
            nt_let = 0x0004,
            nt_block = 0x0005,
            nt_switch = 0x0105,
            nt_comment = 0x0006,
            nt_import = 0x0007,
            nt_wordstat = 0x0008,
            nt_number = 0x0009,
            nt_char = 0x0109,
            nt_string = 0x000a,
            nt_range = 0x000b,
            nt_min_derive_mask = 0x00ff,
            nt_second_derive_mask = 0x0fff,
            nt_max = 0xffff,
        };

        constexpr const char* node_type_to_string(unsigned int type) {
            switch (type) {
                case nt_min:
                    return "min";
                case nt_binary:
                    return "binary";
                case nt_cond:
                    return "cond";
                case nt_if:
                    return "if";
                case nt_for:
                    return "for";
                case nt_switch:
                    return "switch";
                case nt_type:
                    return "type";
                case nt_arrtype:
                    return "arrtype";
                case nt_struct_field:
                    return "struct_field";
                case nt_funcparam:
                    return "funcparam";
                case nt_func:
                    return "func";
                case nt_typedef:
                    return "typedef";
                case nt_let:
                    return "let";
                case nt_block:
                    return "block";
                case nt_comment:
                    return "comment";
                case nt_import:
                    return "import";
                case nt_wordstat:
                    return "wordstat";
                case nt_number:
                    return "number";
                case nt_string:
                    return "string";
                default:
                    return "(unknown)";
            }
        }

#ifdef MINL_Constexpr
#undef MINL_Constexpr
#endif
#define MINL_Constexpr inline

        struct MinNode {
            const unsigned int node_type = 0;
            std::string str;
            Pos pos;
            MINL_Constexpr MinNode() = default;

           protected:
            MINL_Constexpr MinNode(int id)
                : node_type(id) {}
        };

        struct BinaryNode : MinNode {
            std::shared_ptr<MinNode> left;
            std::shared_ptr<MinNode> right;
            MINL_Constexpr BinaryNode()
                : MinNode(nt_binary) {}

           protected:
            MINL_Constexpr BinaryNode(int id)
                : MinNode(id) {}
        };

        struct NumberNode : MinNode {
            bool is_float;
            int radix;
            number::NumErr parsable = false;
            size_t integer = 0;
            double floating = 0;
            MINL_Constexpr NumberNode()
                : MinNode(nt_number) {}

           protected:
            MINL_Constexpr NumberNode(int nt)
                : MinNode(nt) {}
        };

        struct CharNode : NumberNode {
            bool is_utf = false;
            MINL_Constexpr CharNode()
                : NumberNode(nt_char) {}
        };

        struct StringNode : MinNode {
            char prefix;
            char store_c;
            MINL_Constexpr StringNode()
                : MinNode(nt_string) {}
        };

        struct WordStatNode : MinNode {
            std::shared_ptr<MinNode> expr;
            std::shared_ptr<MinNode> block;
            MINL_Constexpr WordStatNode()
                : MinNode(nt_wordstat) {}
        };

        struct CondStatNode : BinaryNode {
            std::shared_ptr<MinNode> block;
            MINL_Constexpr CondStatNode()
                : BinaryNode(nt_cond) {}

           protected:
            MINL_Constexpr CondStatNode(int id)
                : BinaryNode(id) {}
        };

        // Derive CondStatNode
        struct IfStatNode : CondStatNode {
            std::shared_ptr<MinNode> els;
            MINL_Constexpr IfStatNode()
                : CondStatNode(nt_if) {}
        };

        // Derive CondStatNode
        struct ForStatNode : CondStatNode {
            std::shared_ptr<MinNode> center;
            MINL_Constexpr ForStatNode()
                : CondStatNode(nt_for) {}
        };

        struct BlockNode : MinNode {
            std::shared_ptr<BlockNode> next;
            std::shared_ptr<MinNode> element;
            MINL_Constexpr BlockNode()
                : MinNode(nt_block) {}

           protected:
            MINL_Constexpr BlockNode(int id)
                : MinNode(id) {
            }
        };

        // Derive BlockNode
        struct SwitchStatNode : BlockNode {
            std::shared_ptr<MinNode> expr;
            std::shared_ptr<SwitchStatNode> next_case;
            MINL_Constexpr SwitchStatNode()
                : BlockNode(nt_switch) {}
        };

        struct ImportNode : MinNode {
            std::shared_ptr<ImportNode> next;
            std::shared_ptr<MinNode> as;
            std::shared_ptr<MinNode> from;
            MINL_Constexpr ImportNode()
                : MinNode(nt_import) {}
        };

        enum func_expr_mode {
            fe_def,
            fe_tydec,
            fe_expr,
            fe_iface,
        };

        struct TypeNode : MinNode {
            std::shared_ptr<TypeNode> type;
            MINL_Constexpr TypeNode()
                : MinNode(nt_type) {}

           protected:
            MINL_Constexpr TypeNode(int id)
                : MinNode(id) {}
        };

        // Derive TypeNode
        struct ArrayTypeNode : TypeNode {
            std::shared_ptr<MinNode> expr;
            MINL_Constexpr ArrayTypeNode()
                : TypeNode(nt_arrtype) {
            }
        };

        // Derive TypeNode
        struct GenericTypeNode : TypeNode {
            MINL_Constexpr GenericTypeNode()
                : TypeNode(nt_gentype) {}
            std::string type_param;
        };

        // Derive TypeNode
        struct StructFieldNode : TypeNode {
            std::shared_ptr<StructFieldNode> next;
            std::shared_ptr<MinNode> ident;
            std::shared_ptr<MinNode> init;
            MINL_Constexpr StructFieldNode()
                : TypeNode(nt_struct_field) {}
        };

        // Derive TypeNode
        struct FuncParamNode : TypeNode {
            std::shared_ptr<FuncParamNode> next;
            std::shared_ptr<MinNode> ident;
            MINL_Constexpr FuncParamNode()
                : TypeNode(nt_funcparam) {}

           protected:
            MINL_Constexpr FuncParamNode(int id)
                : TypeNode(id) {}
        };

        // Derive FuncParamNode -> TypeNode
        struct FuncNode : FuncParamNode {
            std::shared_ptr<MinNode> return_;
            std::shared_ptr<MinNode> block;
            func_expr_mode mode{};
            MINL_Constexpr FuncNode()
                : FuncParamNode(nt_func) {}
        };

        struct InterfaceNode : TypeNode {
            MINL_Constexpr InterfaceNode()
                : TypeNode(nt_interface) {}
            std::shared_ptr<InterfaceNode> next;
            std::shared_ptr<FuncNode> method;
        };

        struct LetNode : MinNode {
            std::shared_ptr<MinNode> ident;
            std::shared_ptr<TypeNode> type;
            std::shared_ptr<MinNode> init;
            std::shared_ptr<LetNode> next;
            MINL_Constexpr LetNode()
                : MinNode(nt_let) {}
        };

        struct CommentNode : MinNode {
            std::string comment;
            std::shared_ptr<CommentNode> next;
            MINL_Constexpr CommentNode()
                : MinNode(nt_comment) {}
        };

        struct TypeDefNode : MinNode {
            std::shared_ptr<TypeDefNode> next;
            std::shared_ptr<MinNode> ident;
            std::shared_ptr<TypeNode> type;
            MINL_Constexpr TypeDefNode()
                : MinNode(nt_typedef) {}
        };

        struct RangeNode : MinNode {
            std::shared_ptr<NumberNode> min_;
            std::shared_ptr<NumberNode> max_;
            MINL_Constexpr RangeNode()
                : MinNode(nt_range) {}
        };
        // node strs

        constexpr auto comment_str_ = "(comment)";

        constexpr auto type_group_str_ = "(type_group)";
        constexpr auto new_type_str_ = "(new_type)";
        constexpr auto type_alias_str_ = "(type_alias)";

        constexpr auto type_siganture = "(type_signature)";
        constexpr auto pointer_str_ = "(pointer)";
        constexpr auto reference_str_ = "(reference)";
        constexpr auto array_str_ = "[(fixed)]";
        constexpr auto vector_str_ = "[(dynamic)]";
        constexpr auto mut_str_ = "(mutable)";
        constexpr auto const_str_ = "(const)";
        constexpr auto va_arg_str_ = "(variable_argument)";
        constexpr auto typeof_str_ = "(typeof)";
        constexpr auto genr_str_ = "(generic)";

        constexpr auto struct_str_ = "{(struct)}";
        constexpr auto struct_field_str_ = "(struct_field)";
        constexpr auto union_str_ = "{(union)}";
        constexpr auto union_field_str_ = "(union_field)";

        constexpr auto nilstat_str_ = "(nil_statement)";
        constexpr auto return_str_ = "(return)";
        constexpr auto linkage_str_ = "(linkage)";
        constexpr auto break_str_ = "(break)";
        constexpr auto continue_str_ = "(continue)";

        constexpr const char* for_stat_str_[] = {
            "{(for_inf_loop)}",
            "{(for_cond)}",
            "{(for_init_cond)}",
            "{(for_init_cond_do)}",
        };

        constexpr const char* if_stat_str_[] = {
            nullptr,
            "{(if_cond)}",
            "{(if_init_cond)}",
            nullptr,
        };

        constexpr auto switch_str_ = "(switch)";
        constexpr auto case_str_ = "(case)";
        constexpr auto default_str_ = "(default)";

        constexpr auto import_group_str_ = "(import_group)";
        constexpr auto imports_str_ = "(import)";

        constexpr auto block_statement_str_ = "{(block)}";
        constexpr auto block_element_str_ = "(block_element)";

        constexpr auto repeat_str_ = "{(repeat)}";
        constexpr auto repeat_element_str_ = "(repeat_element)";

        constexpr auto program_str_ = "{(program)}";
        constexpr auto program_element_str_ = "(program_element)";

        constexpr auto interface_str_ = "(interface)";
        constexpr auto interface_method_str_ = "(interface_method)";

        constexpr auto let_def_str_ = "(let_def)";
        constexpr auto let_def_group_str_ = "(let_def_group)";
        constexpr auto const_def_str_ = "(const_def)";
        constexpr auto const_def_group_str_ = "(const_def_group)";

        constexpr auto typeprim_str_ = "(type_primitive)";

        constexpr auto func_def_str_ = "(function_define)";
        constexpr auto func_dec_str_ = "(function_declaere)";

        constexpr auto func_param_str_ = "(function_param)";

        constexpr auto func_return_param_str_ = "(function_named_return)";

        // parser utility

        constexpr auto ident_default() {
            return [](auto& seq) -> bool {
                return !seq.eos() &&
                       (!number::is_control_char(seq.current()) &&
                            !number::is_symbol_char(seq.current()) ||
                        seq.current() == '_');
            };
        }

        constexpr auto cond_read(auto&& cond) {
            return [=](auto& str, auto& seq) {
                while (cond(seq)) {
                    str.push_back(seq.current());
                    seq.consume();
                }
                return str.size() != 0;
            };
        }

        inline auto make_ident_node(auto&& name, Pos pos) {
            auto res = std::make_shared<MinNode>();
            res->str = std::move(name);
            res->pos = pos;
            return res;
        }

        constexpr auto ident_default_read = cond_read(ident_default());

        constexpr bool expect_ident(auto& seq, auto ident) {
            const auto start = seq.rptr;
            if (!seq.seek_if(ident)) {
                return false;
            }
            if (ident_default()(seq)) {
                seq.rptr = start;
                return false;
            }
            return true;
        }

        constexpr bool match_ident(auto& seq, auto ident) {
            const auto start = seq.rptr;
            auto res = expect_ident(seq, ident);
            seq.rptr = start;
            return res;
        }

        constexpr bool match_typeprefix(auto& seq) {
            return seq.match("*") || seq.match("[") || seq.match("&") ||
                   match_ident(seq, "struct") || match_ident(seq, "union") || match_ident(seq, "fn") ||
                   match_ident(seq, "mut") || match_ident(seq, "const") || match_ident(seq, "genr") ||
                   match_ident(seq, "interface") || match_ident(seq, "typeof") || seq.match(")") ||
                   seq.match("...");
        }

        constexpr auto idents(auto& str) {
            return view::make_ref_splitview(str, ",");
        }

        constexpr auto read_parameter(auto& seq, auto& param_end, auto& param_name, bool& no_param_name) {
            const auto param_start = seq.rptr;
            while (true) {
                // (*Arg,ident Arg) -> ok (type,name type)
                // (Arg1,Arg2,Arg3) -> ok (type,type,type)
                // (ident1,ident2,ident3 Arg) -> ok (name,name,name type)
                // (Arg,ident Arg) -> ok but... (name,name type)
                // (type Arg,ident Arg) -> ok (type,ident type)
                // (Arg,type Arg,Arg) -> ok (type,type,type)
                // (type Arg,Arg,Arg) -> ok (type,type,type)
                // (Arg,Arg,Arg type Arg) -> ok (name,name,name type)
                // (ident,type ident Arg) -> bad (name,type? type?)
                // (ident.Type) -> ok (type)
                // (ident user.Type,user.Type) -> ok (ident type,type)
                // (Type,Type,user.Type) -> ok (type,type,type)
                if (match_typeprefix(seq)) {
                    seq.rptr = param_start;
                    no_param_name = true;
                    break;
                }
                if (expect_ident(seq, "type")) {
                    if (param_name.size()) {
                        seq.rptr = param_start;
                    }
                    no_param_name = true;
                    break;
                }
                if (!ident_default_read(param_name, seq)) {
                    return false;
                }
                param_end = seq.rptr;
                space::consume_space(seq, true);
                if (seq.seek_if(",")) {
                    // expect multiple param_name in same type
                    param_name.push_back(',');
                    space::consume_space(seq, true);
                    continue;
                }
                if (seq.match(")")) {
                    seq.rptr = param_start;
                    no_param_name = true;
                    break;
                }
                auto tmp = seq.rptr;
                if (seq.seek_if(".") && (space::consume_space(seq, true), ident_default()(seq))) {
                    seq.rptr = param_start;
                    no_param_name = true;
                    break;
                }
                seq.rptr = tmp;
                break;
            }
            if (expect_ident(seq, "type")) {
                space::consume_space(seq, true);
            }
            return true;
        }

        // log util
        template <class ErrC, class Str>
        struct log_raii {
            ErrC& errc;
            Str str;
            const char* file;
            int line;
            constexpr log_raii(ErrC& c, Str s, const char* file, int line)
                : errc(c), str(s), file(file), line(line) {
                errc.log_enter(s, file, line);
            }

            constexpr ~log_raii() {
                errc.log_leave(str, file, line);
            }
        };

#define MINL_FUNC_LOG(FUNC) auto log_object____ = log_raii(p.errc, FUNC, __FILE__, __LINE__);
#define MINL_FUNC_LOG_OLD(FUNC) auto log_object____ = log_raii(errc, FUNC, __FILE__, __LINE__);
    }  // namespace minilang
}  // namespace utils
