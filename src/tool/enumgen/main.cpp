/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <comb2/basic/group.h>
#include <comb2/basic/logic.h>
#include <comb2/basic/literal.h>
#include <comb2/composite/comment.h>
#include <comb2/composite/string.h>
#include <comb2/tree/branch_table.h>
#include <comb2/tree/simple_node.h>
#include <optional>
#include <file/file_view.h>
#include <helper/expected.h>
#include <code/code_writer.h>
#include <variant>
#include <comb2/basic/proxy.h>

namespace parser {
    using namespace futils::comb2;
    using namespace ops;
    namespace cps = composite;
    constexpr auto space = cps::space | cps::tab | cps::eol;
    constexpr auto ident = cps::c_ident;
    constexpr auto tag_enum = "enum";
    constexpr auto tag_enum_name = "enum_name";
    constexpr auto tag_enum_fields = "enum_fields";
    constexpr auto tag_enum_field = "enum_field";
    constexpr auto tag_enum_field_name = "enum_field_name";
    constexpr auto tag_enum_value = "enum_value";
    constexpr auto tag_type_args = "type_args";
    constexpr auto tag_type_arg = "type_arg";
    constexpr auto tag_type_alias = "type_alias";
    constexpr auto tag_type_alias_name = "type_alias_name";
    constexpr auto tag_type_alias_type = "type_alias_type";
    constexpr auto tag_namespace = "namespace";
    constexpr auto tag_namespace_name = "namespace_name";
    constexpr auto tag_namespace_elems = "namespace_elems";

    constexpr auto tag_use = "use";
    constexpr auto tag_use_path = "use_path";

    constexpr auto maybe_comma = -(lit(',') & *space);

    constexpr auto type_args = group(tag_type_args,
                                     lit('<') &
                                         *space & *(str(tag_type_arg, ident) & *space & maybe_comma) & +lit('>') & *space);

    constexpr auto enum_begin = lit("enum") & ~space & +str(tag_enum_name, ident) & *space & -type_args & lit("{") & *space;
    constexpr auto enum_end = lit("}") & *space;
    constexpr auto until_line = *(not_(cps::eol) & any);
    constexpr auto type_alias = group(tag_type_alias, lit("type") & ~space & +str(tag_type_alias_name, ident) & *space & -type_args &
                                                          str(tag_type_alias_type, until_line) & *space);

    constexpr auto include_ = group(tag_use, lit("use") & *space & str(tag_use_path, until_line) & *space);

    constexpr auto elems = method_proxy(elements);
    constexpr auto ident_and_colons = -lit("::") & ident & *(*space & lit("::") & *space & +ident);
    constexpr auto namespace_ = group(tag_namespace, lit("mod") & ~space & +str(tag_namespace_name, ident_and_colons) & *space & lit("{") & *space & group(tag_namespace_elems, elems) & +lit("}") & *space);

    constexpr auto enum_typ_val = str(tag_enum_value, *(not_(lit(')')) & any));
    constexpr auto enum_body = group(
        tag_enum_field,
        str(tag_enum_field_name, ident) & *space & -(lit('(') & enum_typ_val & +lit(')') & *space) & maybe_comma);
    constexpr auto enum_ = group(tag_enum, enum_begin& group(tag_enum_fields, *enum_body) & +enum_end);

    constexpr auto elements = *(type_alias | enum_ | namespace_ | include_);
    constexpr auto program = *space & elems & +eos;

    constexpr auto make_parser() {
        struct L {
            decltype(elements) elements;
        } l{elements};
        return [=](auto&& seq, auto&& ctx) {
            return program(seq, ctx, l);
        };
    }

    constexpr auto parser = make_parser();

    namespace test {
        constexpr auto str = R"(
type Fn<U> void(*)(U*)
enum Test<A, B, C, D, E> {
    A(A),
    B(B),
    C(C)
    D,
    E
}
mod test {
    type Test2<A> A
    enum Test3 {
        A,
        B,
        C
    }
}
        )";
        constexpr bool test() {
            auto seq = futils::make_ref_seq(str);
            auto ctx = futils::comb2::test::TestContext{};
            return parser(seq, ctx) == futils::comb2::Status::match;
        }

        static_assert(test());
    }  // namespace test

}  // namespace parser

struct EnumField {
    std::string name;
    std::string type;
};

struct Enum {
    std::string name;
    std::vector<std::string> type_args;
    std::vector<EnumField> fields;
};

struct TypeAlias {
    std::string name;
    std::vector<std::string> type_args;
    std::string type;
};

struct Include {
    std::string path;
};
struct Namespace {
    std::string name;
    std::vector<std::variant<Enum, TypeAlias, Namespace, Include>> elems;
};

using Elem = std::variant<Enum, TypeAlias, Namespace, Include>;
namespace tree = futils::comb2::tree;
void parse_r(std::vector<Elem>& elems, const std::shared_ptr<tree::node::Node>& elem) {
    if (elem->tag == parser::tag_use) {
        auto use = tree::node::as_group(elem);
        if (!use) {
            return;
        }
        auto path = tree::node::as_tok(use->children[0]);
        if (!path) {
            return;
        }
        Include i;
        i.path = path->token;
        elems.push_back(std::move(i));
        return;
    }
    if (elem->tag == parser::tag_namespace) {
        auto ns = tree::node::as_group(elem);
        if (!ns) {
            return;
        }
        auto name = tree::node::as_tok(ns->children[0]);
        if (!name) {
            return;
        }
        Namespace n;
        n.name = name->token;
        auto i = 1;
        auto elems_group = tree::node::as_group(ns->children[i]);
        if (!elems_group) {
            return;
        }
        for (auto& elem : elems_group->children) {
            parse_r(n.elems, elem);
        }
        elems.push_back(std::move(n));
        return;
    }
    if (elem->tag == parser::tag_type_alias) {
        auto type_alias = tree::node::as_group(elem);
        if (!type_alias) {
            return;
        }
        auto name = tree::node::as_tok(type_alias->children[0]);
        if (!name) {
            return;
        }
        TypeAlias ta;
        ta.name = name->token;
        auto i = 1;
        if (type_alias->children[1]->tag == parser::tag_type_args) {
            auto type_args = tree::node::as_group(type_alias->children[1]);
            if (!type_args) {
                return;
            }
            for (auto& arg : type_args->children) {
                auto arg_tok = tree::node::as_tok(arg);
                if (!arg_tok) {
                    continue;
                }
                ta.type_args.push_back(arg_tok->token);
            }
            i++;
        }
        auto type = tree::node::as_tok(type_alias->children[i]);
        if (!type) {
            return;
        }
        ta.type = type->token;
        elems.push_back(std::move(ta));
        return;
    }
    auto enum_ = tree::node::as_group(elem);
    if (!enum_) {
        return;
    }
    auto name = tree::node::as_tok(enum_->children[0]);
    if (!name) {
        return;
    }
    Enum e;
    e.name = name->token;
    auto i = 1;
    if (enum_->children[1]->tag == parser::tag_type_args) {
        auto type_args = tree::node::as_group(enum_->children[1]);
        if (!type_args) {
            return;
        }
        for (auto& arg : type_args->children) {
            auto arg_tok = tree::node::as_tok(arg);
            if (!arg_tok) {
                continue;
            }
            e.type_args.push_back(arg_tok->token);
        }
        i++;
    }
    auto fields = tree::node::as_group(enum_->children[i]);
    if (!fields) {
        return;
    }

    for (auto& field : fields->children) {
        auto f = tree::node::as_group(field);
        if (!f) {
            continue;
        }
        auto field_name = tree::node::as_tok(f->children[0]);
        if (!field_name) {
            continue;
        }
        EnumField ef;
        ef.name = field_name->token;
        if (f->children.size() > 1) {
            auto field_value = tree::node::as_tok(f->children[1]);
            if (!field_value) {
                continue;
            }
            ef.type = field_value->token;
        }
        e.fields.push_back(std::move(ef));
    }
    elems.push_back(std::move(e));
}

std::optional<std::vector<Elem>> parse(auto&& text) {
    auto seq = futils::make_ref_seq(text);
    parser::tree::BranchTable table;
    if (parser::parser(seq, table) != parser::Status::match) {
        return std::nullopt;
    }
    auto group = tree::node::collect(table.root_branch);
    if (!group) {
        return std::nullopt;
    }
    std::vector<Elem> elems;
    for (auto& elem : group->children) {
        parse_r(elems, elem);
    }
    return elems;
}

struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string_view> args;
    bool format = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&format, "f,format", "format input file");
    }
};
auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();

std::string to_lower_snake(std::string_view str) {
    std::string out;
    out.reserve(str.size());
    bool first = true;
    for (auto c : str) {
        if (std::isupper(c)) {
            if (!first) {
                out.push_back('_');
            }
            out.push_back(std::tolower(c));
        }
        else {
            out.push_back(c);
        }
        first = false;
    }
    return out;
}

using Writer = futils::code::CodeWriter<std::string>;

auto write_template_arguments(Writer& w, std::vector<std::string>& type_args) {
    if (type_args.size() > 0) {
        w.write("template <");
        bool first = true;
        for (auto& arg : type_args) {
            if (!first) {
                w.write(", ");
            }
            w.write("typename ", arg);
            first = false;
        }
        w.writeln(">");
    }
};

void write(Writer& w, Elem& elem) {
    if (auto include = std::get_if<Include>(&elem)) {
        w.writeln("#include ", include->path);
        return;
    }
    if (auto ns = std::get_if<Namespace>(&elem)) {
        w.writeln("namespace ", ns->name, " {");
        {
            auto i = w.indent_scope();
            for (auto& elem : ns->elems) {
                write(w, elem);
            }
        }
        w.writeln("}");
        return;
    }
    if (auto type_alias = std::get_if<TypeAlias>(&elem)) {
        write_template_arguments(w, type_alias->type_args);
        w.writeln("using ", type_alias->name, " = ", type_alias->type, ";");
        return;
    }
    auto enum_ = *std::get_if<Enum>(&elem);
    std::string calc_biggest_size;
    std::string calc_biggest_align;
    std::string all_copyable;
    std::string all_movable;
    w.writeln("enum class ", enum_.name, "Tag {");
    {
        auto i = w.indent_scope();
        for (auto& field : enum_.fields) {
            w.writeln(field.name, ",");
            if (field.type.empty()) {
                continue;
            }
            if (calc_biggest_size.empty()) {
                calc_biggest_size = "sizeof(" + field.type + ")";
                calc_biggest_align = "alignof(" + field.type + ")";
                all_copyable = "std::is_copy_constructible_v<" + field.type + ">";
                all_movable = "std::is_move_constructible_v<" + field.type + ">";
            }
            else {
                calc_biggest_size = "(" + calc_biggest_size + " > sizeof(" + field.type + ")) ? " + calc_biggest_size +
                                    " : sizeof(" + field.type + ")";
                calc_biggest_align = "(" + calc_biggest_align + " > alignof(" + field.type + ")) ? " +
                                     calc_biggest_align + " : alignof(" + field.type + ")";
                all_copyable += " && std::is_copy_constructible_v<" + field.type + ">";
            }
        }
    }
    w.writeln("};");
    write_template_arguments(w, enum_.type_args);
    w.writeln("struct ", enum_.name, " {");
    {
        auto i = w.indent_scope();
        w.writeln("static constexpr size_t size = ", calc_biggest_size, ";");
        w.writeln("static constexpr size_t align = ", calc_biggest_align, ";");
        w.writeln("private:");
        w.writeln(enum_.name, "Tag tag;");
        w.writeln("union {");
        {
            auto i = w.indent_scope();
            w.writeln("alignas(align) char data[size]{};");
            for (auto& field : enum_.fields) {
                if (!field.type.empty()) {
                    w.writeln(field.type, " ", field.name, "_;");
                }
            }
        }
        w.writeln("};");
        w.writeln("void destruct() noexcept {");

        {
            auto i = w.indent_scope();
            w.writeln("switch(tag) {");
            for (auto& field : enum_.fields) {
                w.writeln("case ", enum_.name, "Tag::", field.name, ":");
                if (!field.type.empty()) {
                    w.indent_writeln("std::destroy_at(std::addressof(", field.name, "_));");
                }
                else {
                    w.indent_writeln("// nothing to destruct");
                }
                w.indent_writeln("break;");
            }
            w.writeln("}");
        }
        w.writeln("}");
        w.writeln("public:");
        w.writeln("~", enum_.name, "() noexcept { destruct(); }");
        for (auto& field : enum_.fields) {
            w.write("struct ", field.name, "{");
            if (!field.type.empty()) {
                w.writeln();
                w.indent_writeln(field.type, " ", "value;");
            }
            w.writeln("};");

            w.write(enum_.name, "(", field.name, " ", field.name, "_) : tag(", enum_.name, "Tag::", field.name, ") {");
            if (!field.type.empty()) {
                w.writeln();
                w.indent_writeln("new (data) ", field.type, "{std::move(", field.name, "_.value)};");
            }
            w.writeln("}");
        }
        for (auto& field : enum_.fields) {
            auto lower_field = to_lower_snake(field.name);
            w.writeln("constexpr bool is_", lower_field, "() const noexcept { return tag == ", enum_.name, "Tag::", field.name, "; }");
            if (!field.type.empty()) {
                w.writeln("constexpr auto* ", lower_field, "() noexcept { return is_", lower_field, "() ? std::addressof(", field.name, "_) : nullptr; }");
            }
            else {
                w.writeln("constexpr bool ", lower_field, "() const noexcept { return is_", lower_field, "(); }");
            }
        }
        w.writeln("constexpr ", enum_.name, "Tag ", "get_tag() const noexcept { return tag; }");

        w.writeln(enum_.name, "& operator=(", enum_.name, "&& other) noexcept {");
        {
            auto i = w.indent_scope();
            w.writeln("if (this == &other) return *this;");
            w.writeln("destruct();");
            w.writeln("tag = other.tag;");
            w.writeln("switch(tag) {");
            for (auto& field : enum_.fields) {
                w.writeln("case ", enum_.name, "Tag::", field.name, ":");
                if (!field.type.empty()) {
                    w.indent_writeln("new (data) ", field.type, "{std::move(other.", field.name, "_)};");
                }
                w.indent_writeln("break;");
            }
            w.writeln("}");
            w.writeln("return *this;");
        }
        w.writeln("}");
    }
    w.writeln("};");
}

void write_format(Writer& w, Elem& elem) {
    if (auto include = std::get_if<Include>(&elem)) {
        w.writeln("use ", include->path);
        return;
    }
    if (auto enum_ = std::get_if<Enum>(&elem)) {
        w.write("enum ", enum_->name);
        if (enum_->type_args.size() > 0) {
            w.write("<");
            for (auto& arg : enum_->type_args) {
                w.write(arg, ", ");
            }
            w.write(">");
        }
        w.writeln(" {");
        {
            auto i = w.indent_scope();
            for (auto& field : enum_->fields) {
                w.write(field.name);
                if (!field.type.empty()) {
                    w.write("(", field.type, ")");
                }
                w.writeln(",");
            }
        }
        w.writeln("};");
    }
    else if (auto type_alias = std::get_if<TypeAlias>(&elem)) {
        w.write("type ", type_alias->name);
        if (type_alias->type_args.size() > 0) {
            w.write("<");
            for (auto& arg : type_alias->type_args) {
                w.write(arg, ", ");
            }
            w.write(">");
        }
        w.writeln(" ", type_alias->type, ";");
    }
    else if (auto ns = std::get_if<Namespace>(&elem)) {
        w.writeln("mod ", ns->name, " {");
        {
            auto i = w.indent_scope();
            for (auto& elem : ns->elems) {
                write_format(w, elem);
            }
        }
        w.writeln("}");
    }
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (flags.args.empty()) {
        cout << "Usage: enumgen <file>\n";
        return 1;
    }
    futils::file::View view;
    if (!view.open(flags.args[0])) {
        cerr << "Failed to open file: " << flags.args[0] << '\n';
        return 1;
    }
    auto e = parse(view);
    if (!e) {
        cerr << "Failed to parse file: " << flags.args[0] << '\n';
        return 1;
    }
    Writer w;
    if (flags.format) {
        for (auto& elem : *e) {
            write_format(w, elem);
        }
        cout << w.out();
        return 0;
    }
    w.writeln("// Code generated by enumgen. DO NOT EDIT.");
    w.writeln("#pragma once");
    w.writeln("#include <cstddef>");
    w.writeln("#include <type_traits>");
    w.writeln("#include <utility>");
    w.writeln("#include <memory>");
    for (auto& elem : *e) {
        write(w, elem);
    }
    cout << w.out();
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
