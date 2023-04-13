/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "binp.h"
#include <regex>

namespace binp {

    bool generate_ident_type(Env& env, const std::string& type) {
        auto& out = env.out;
        if (type == "byte" || type == "uint8") {
            out.write("::std::uint8_t");
        }
        else if (type == "uint16") {
            out.write("::std::uint16_t");
        }
        else if (type == "uint" || type == "uint32") {
            out.write("::std::uint32_t");
        }
        else if (type == "uint64") {
            out.write("::std::uint64_t");
        }
        else {
            return false;
        }
        return true;
    }

    bool generate_default_value(Env& env, const Type& type) {
        env.out.write(" = ");
        if (type.attrs.default_value.size()) {
            if (type.attrs.default_value.find(";") != std::string::npos) {
                env.out.write("(", type.attrs.default_value, ")");
            }
            else {
                env.out.write(type.attrs.default_value);
            }
        }
        else {
            env.out.write("{}");
        }
        env.out.writeln(";");
        return true;
    }

    bool generate_field(Env& env, const Field& field);

    void trim_line(std::string& unform) {
        while (unform.size() && (unform.front() == '\n' || unform.front() == '\r')) {
            unform.erase(0, 1);
        }
        while (unform.size() && (unform.back() == ' ' || unform.back() == '\n' || unform.back() == '\r')) {
            unform.pop_back();
        }
    }

    std::string format_method(const std::string& method, const std::string& do_call) {
        auto unform = std::regex_replace(method, std::regex("invoke_callback"), do_call);
        unform = std::regex_replace(method, std::regex(R"a(invoke_callback\((.*),.*"(.*)".*\))a"), R"a(callback($1, "$2"))a");
        trim_line(unform);
        return unform;
    }

    bool generate_functions(Env& env, const Struct& struct_) {
        auto& out = env.out;
        out.writeln("auto apply_encode(auto&& callback) {");
        {
            const auto block = out.indent_scope();
            for (auto& field : struct_.fields) {
                const auto do_call = "callback(" + field.name + ", \"" + field.name + "\")";
                if (field.type.attrs.encode_method.size()) {
                    auto unform = format_method(field.type.attrs.encode_method, do_call);
                    out.write_unformated(unform);
                    out.line();
                }
                else {
                    out.writeln(do_call, ";");
                }
            }
        }
        out.writeln("}");
        out.line();
        out.writeln("auto apply_decode(auto&& callback) {");
        {
            const auto block = out.indent_scope();
            for (auto& field : struct_.fields) {
                const auto do_call = "callback(" + field.name + ", \"" + field.name + "\")";
                if (field.type.attrs.decode_method.size()) {
                    auto unform = std::regex_replace(field.type.attrs.decode_method, std::regex("invoke_callback"), do_call);
                    trim_line(unform);
                    out.write_unformated(unform);
                    out.line();
                }
                else {
                    out.writeln(do_call, ";");
                }
            }
        }
        out.writeln("}");
        return true;
    }

    bool generate_inner_struct(Env& env, const Type& type) {
        auto& struct_ = std::get<1>(type.type);
        auto& out = env.out;
        out.writeln("{");
        {
            const auto block = out.indent_scope();
            for (auto& field : struct_.fields) {
                if (!generate_field(env, field)) {
                    return false;
                }
            }
            out.line();
            if (!generate_functions(env, struct_)) {
                return false;
            }
        }
        out.write("}");
        return true;
    }

    bool generate_field(Env& env, const Field& field) {
        if (field.type.type.index() == 0) {
            if (!generate_ident_type(env, std::get<0>(field.type.type).name)) {
                return false;
            }
        }
        else if (field.type.type.index() == 1) {
            env.out.write("struct ");
            if (!generate_inner_struct(env, field.type)) {
                return false;
            }
        }
        else {
            return false;
        }
        env.out.write(" ", field.name);
        if (!generate_default_value(env, field.type)) {
            return false;
        }
        return true;
    }

    bool generate_struct(Env& env, const TypeDef& def) {
        auto& struct_ = std::get<1>(def.base.type);
        auto& out = env.out;
        out.write("struct ", def.name, " ");
        if (!generate_inner_struct(env, def.base)) {
            return false;
        }
        out.writeln(";");
        out.line();
        return true;
    }

    bool generate_alias(Env& env, const TypeDef& def) {
        auto& ident = std::get<0>(def.base.type);
        return true;
    }

    bool generate_inner(Env& env, const Program& prog) {
        for (auto& type : prog.typedefs) {
            if (type.base.type.index() == 1) {
                if (!generate_struct(env, type)) {
                    return false;
                }
            }
            else if (type.base.type.index() == 0) {
                if (!generate_alias(env, type)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool generate(Env& env, const Program& prog) {
        auto& out = env.out;
        out.writeln("/*license*/");
        out.writeln("// Code generated by binp (https://github.com/on-keyday/utils). DO NOT EDIT!");
        out.writeln("#include <cstdint>");
        for (auto& inc : prog.includes) {
            out.writeln("#include ", inc);
        }
        out.line();
        if (prog.namespace_.size()) {
            out.writeln("namespace ", prog.namespace_, " {");
            {
                const auto block = out.indent_scope();
                if (!generate_inner(env, prog)) {
                    return false;
                }
            }
            out.writeln("}");
        }
        else {
            if (!generate_inner(env, prog)) {
                return false;
            }
        }
        return true;
    }
}  // namespace binp
