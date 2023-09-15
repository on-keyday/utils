/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "istr.h"

namespace utils::wasm::section {
    enum class ID : byte {
        custom,
        type,
        import_,
        function,
        table,
        memory,
        global,
        export_,
        start,
        element,
        code,
        data,
        data_count,
    };

    struct SectionHeader {
        ID id = ID::custom;
        std::uint32_t len = 0;

        constexpr result<void> parse(binary::reader& r) noexcept {
            return read_byte(r).and_then([&](byte b) {
                id = ID(b);
                return parse_uint(r, len);
            });
        }

        constexpr result<void> render(binary::writer& w) const noexcept {
            return write_byte(w, byte(id)).and_then([&] {
                return render_uint(w, len);
            });
        }
    };

    struct Type {
        std::vector<type::FunctionType> funcs;

        constexpr result<void> parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return type::parse_function_type(r).transform(push_back_to(funcs));
            });
        }

        constexpr result<void> render(binary::writer& w) const {
            return render_vec(w, funcs.size(), [&](std::uint32_t i) {
                return funcs[i].render(w);
            });
        }
    };

    struct Custom {
        view::rvec name;
        view::rvec data;

        constexpr result<void> parse(binary::reader& r) {
            return parse_name(r, name).and_then([&]() -> result<void> {
                r.read(data, r.remain().size());
                return {};
            });
        }

        constexpr result<void> render(binary::writer& w) const {
            return render_name(w, name).and_then([&]() -> result<void> {
                if (!w.write(data)) {
                    return unexpect(Error::short_buffer);
                }
                return {};
            });
        }
    };

    struct Unspec {
        view::rvec data;

        constexpr result<void> parse(binary::reader& r) {
            r.read(data, r.remain().size());
            return {};
        }

        constexpr result<void> render(binary::writer& w) const {
            if (!w.write(data)) {
                return unexpect(Error::short_buffer);
            }
            return {};
        }
    };

    enum class PortType {
        func,
        table,
        mem,
        global,
    };

    struct ImportDesc {
        PortType type;
        std::variant<Index, type::TableType, type::MemoryType, type::GlobalType> desc;
        template <class T>
        result<const T*> as() const {
            auto res = std::get_if<T>(&desc);
            if (!res) {
                return unexpect(Error::unexpected_import_desc);
            }
            return res;
        }

        constexpr result<void> parse(binary::reader& r) {
            return read_byte(r).and_then([&](byte t) -> result<void> {
                type = PortType(t);
                switch (type) {
                    case PortType::func:
                        return parse_uint<std::uint32_t>(r).transform(assign_to(desc));
                    case PortType::table:
                        return type::parse_table_type(r).transform(assign_to(desc));
                    case PortType::mem:
                        return type::parse_memory_type(r).transform(assign_to(desc));
                    case PortType::global:
                        return type::parse_global_type(r).transform(assign_to(desc));
                    default:
                        return unexpect(Error::unexpected_import_desc);
                }
            });
        }

        constexpr result<void> render(binary::writer& w) const {
            return write_byte(w, byte(type)).and_then([&]() -> result<void> {
                switch (type) {
                    case PortType::func:
                        return as<Index>().and_then([&](auto v) { return render_uint(w, *v); });
                    case PortType::table:
                        return as<type::TableType>().and_then([&](auto v) { return v->render(w); });
                    case PortType::mem:
                        return as<type::MemoryType>().and_then([&](auto v) { return v->render(w); });
                    case PortType::global:
                        return as<type::GlobalType>().and_then([&](auto v) { return v->render(w); });
                    default:
                        return unexpect(Error::unexpected_import_desc);
                }
            });
        }
    };

    struct Import {
        view::rvec mod;
        view::rvec name;
        ImportDesc desc;

        constexpr auto parse(binary::reader& r) {
            return parse_name(r, mod)
                .and_then([&] {
                    return parse_name(r, name);
                })
                .and_then([&] {
                    return desc.parse(r);
                });
        }

        constexpr auto render(binary::writer& w) const {
            return render_name(w, mod)
                .and_then([&] { return render_name(w, name); })
                .and_then([&] { return desc.render(w); });
        }
    };

    template <class T>
    auto parse_vec_elm(binary::reader& r, auto& vec) {
        T global;
        return global.parse(r)
            .transform([&] { return global; })
            .transform(push_back_to(vec));
    }

    struct Imports {
        std::vector<Import> imports;
        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Import>(r, imports);
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, imports.size(), [&](auto i) {
                return imports[i].render(w);
            });
        }
    };

    struct Function {
        std::vector<Index> funcs;
        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_uint<std::uint32_t>(r).transform(push_back_to(funcs));
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, funcs.size(), [&](auto i) {
                return render_uint(w, funcs[i]);
            });
        }
    };

    struct Table {
        std::vector<type::TableType> tables;
        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return type::parse_table_type(r).transform(push_back_to(tables));
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, tables.size(), [&](auto i) {
                return tables[i].render(w);
            });
        }
    };

    struct Memory {
        std::vector<type::MemoryType> memories;
        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return type::parse_memory_type(r).transform(push_back_to(memories));
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, memories.size(), [&](auto i) {
                return memories[i].render(w);
            });
        }
    };

    struct Global {
        type::GlobalType type;
        code::Expr expr;
        auto parse(binary::reader& r) {
            return type::parse_global_type(r)
                .transform(assign_to(type))
                .and_then([&] { return code::parse_expr(r); })
                .transform(assign_to(expr));
        }

        auto render(binary::writer& w) const {
            return type.render(w).and_then([&] {
                return code::render_expr(expr, w);
            });
        }
    };

    struct Globals {
        std::vector<Global> globals;
        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Global>(r, globals);
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, globals.size(), [&](auto i) {
                return globals[i].render(w);
            });
        }
    };

    struct ExportDesc {
        PortType port{};
        Index index = 0;

        constexpr auto parse(binary::reader& r) {
            return read_byte(r)
                .transform(cast_to<PortType>(port))
                .and_then([&] { return parse_uint<std::uint32_t>(r); })
                .transform(assign_to(index));
        }

        constexpr auto render(binary::writer& w) const {
            return write_byte(w, byte(port)).and_then([&] { return render_uint(w, index); });
        }
    };

    struct Export {
        view::rvec name;
        ExportDesc desc;

        constexpr auto parse(binary::reader& r) {
            return parse_name(r, name).and_then([&] { return desc.parse(r); });
        }

        constexpr auto render(binary::writer& w) const {
            return render_name(w, name).and_then([&] { return desc.render(w); });
        }
    };

    struct Exports {
        std::vector<Export> exports;
        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Export>(r, exports);
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, exports.size(), [&](auto i) {
                return exports[i].render(w);
            });
        }
    };

    struct Exprs {
        std::vector<code::Expr> exprs;

        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return code::parse_expr(r).transform(push_back_to(exprs));
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, exprs.size(), [&](auto i) {
                return code::render_expr(exprs[i], w);
            });
        }
    };

    struct Start {
        Index func = 0;
        constexpr auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r, func);
        }

        constexpr auto render(binary::writer& w) const {
            return render_uint(w, func);
        }
    };

    // Element0
    struct Element0 {
        code::Expr expr;
        Function funcs;

        auto parse(binary::reader& r) {
            return code::parse_expr(r)
                .and_then([&](code::Expr e) {
                    expr = std::move(e);
                    return funcs.parse(r);
                });
        }

        auto render(binary::writer& w) const {
            return code::render_expr(expr, w).and_then([&] {
                return funcs.render(w);
            });
        }
    };

    // Element1
    struct Element1 {
        byte kind = 0;
        Function funcs;

        auto parse(binary::reader& r) {
            return read_byte(r)
                .and_then([&](byte k) {
                    kind = k;
                    return funcs.parse(r);
                });
        }

        auto render(binary::writer& w) const {
            return write_byte(w, kind).and_then([&] {
                return funcs.render(w);
            });
        }
    };

    // Element2
    struct Element2 {
        Index table_index = 0;
        code::Expr expr;
        byte kind = 0;
        Function funcs;

        auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r)
                .and_then([&](Index idx) {
                    table_index = idx;
                    return code::parse_expr(r);
                })
                .and_then([&](code::Expr&& e) {
                    expr = std::move(e);
                    return read_byte(r);
                })
                .and_then([&](byte k) {
                    kind = k;
                    return funcs.parse(r);
                });
        }

        auto render(binary::writer& w) const {
            return render_uint(w, table_index)
                .and_then([&] {
                    return code::render_expr(expr, w);
                })
                .and_then([&] {
                    return write_byte(w, kind);
                })
                .and_then([&] {
                    return funcs.render(w);
                });
        }
    };

    // Element3
    struct Element3 {
        byte kind = 0;
        Function funcs;

        auto parse(binary::reader& r) {
            return read_byte(r)
                .and_then([&](byte k) {
                    kind = k;
                    return funcs.parse(r);
                });
        }

        auto render(binary::writer& w) const {
            return write_byte(w, kind).and_then([&] {
                return funcs.render(w);
            });
        }
    };

    // Element4
    struct Element4 {
        code::Expr expr;
        Exprs exprs;

        auto parse(binary::reader& r) {
            return code::parse_expr(r)
                .and_then([&](code::Expr e) {
                    expr = std::move(e);
                    return exprs.parse(r);
                });
        }

        auto render(binary::writer& w) const {
            return code::render_expr(expr, w).and_then([&] {
                return exprs.render(w);
            });
        }
    };

    // Element5
    struct Element5 {
        type::Type reftype;
        Exprs exprs;

        auto parse(binary::reader& r) {
            return type::parse_reftype(r)
                .and_then([&](type::Type t) {
                    reftype = std::move(t);
                    return exprs.parse(r);
                });
        }

        auto render(binary::writer& w) const {
            return type::render_reftype(w, reftype).and_then([&] {
                return exprs.render(w);
            });
        }
    };

    // Element6
    struct Element6 {
        Index table_index = 0;
        code::Expr expr;
        type::Type reftype;
        Exprs exprs;

        auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r)
                .and_then([&](Index idx) {
                    table_index = idx;
                    return code::parse_expr(r);
                })
                .and_then([&](code::Expr e) {
                    expr = std::move(e);
                    return type::parse_reftype(r);
                })
                .and_then([&](type::Type t) {
                    reftype = std::move(t);
                    return exprs.parse(r);
                });
        }

        auto render(binary::writer& w) const {
            return render_uint(w, table_index)
                .and_then([&] {
                    return code::render_expr(expr, w);
                })
                .and_then([&] {
                    return type::render_reftype(w, reftype);
                })
                .and_then([&] {
                    return exprs.render(w);
                });
        }
    };

    // Element7
    struct Element7 {
        type::Type reftype;
        Exprs exprs;

        auto parse(binary::reader& r) {
            return type::parse_reftype(r)
                .and_then([&](type::Type t) {
                    reftype = std::move(t);
                    return exprs.parse(r);
                });
        }

        auto render(binary::writer& w) const {
            return type::render_reftype(w, reftype).and_then([&] {
                return exprs.render(w);
            });
        }
    };

    struct Element {
        std::variant<Element0, Element1, Element2, Element3, Element4, Element5, Element6, Element7> element;
        auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r).and_then([&](std::uint32_t i) -> result<void> {
                switch (i) {
                    default:
                        return unexpect(Error::unexpected_instruction);
                    case 0:
                        element = Element0{};
                        break;
                    case 1:
                        element = Element1{};
                        break;
                    case 2:
                        element = Element2{};
                        break;
                    case 3:
                        element = Element3{};
                        break;
                    case 4:
                        element = Element4{};
                        break;
                    case 5:
                        element = Element5{};
                        break;
                    case 6:
                        element = Element6{};
                        break;
                    case 7:
                        element = Element7{};
                        break;
                }
                return std::visit([&](auto& e) {
                    return e.parse(r);
                },
                                  element);
            });
        }

        auto render(binary::writer& w) const {
            return render_uint(w, element.index()).and_then([&] {
                return std::visit([&](auto& e) {
                    return e.render(w);
                },
                                  element);
            });
        }
    };

    struct Elements {
        std::vector<Element> elements;
        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Element>(r, elements);
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, elements.size(), [&](auto i) {
                return elements[i].render(w);
            });
        }
    };

    struct Locals {
        std::uint32_t count = 0;
        type::Type valtype;

        constexpr auto parse(binary::reader& r) {
            return parse_uint(r, count)
                .and_then([&] { return type::parse_valtype(r); })
                .transform(assign_to(valtype));
        }

        constexpr auto render(binary::writer& w) const {
            return render_uint(w, count).and_then([&] {
                return type::render_valtype(w, valtype);
            });
        }
    };

    struct Func {
        std::vector<Locals> locals;
        code::Expr expr;

        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                       return parse_vec_elm<Locals>(r, locals);
                   })
                .and_then([&] { return code::parse_expr(r); })
                .transform(assign_to(expr));
        }

        constexpr auto render(binary::writer& w) const {
            return render_vec(w, locals.size(), [&](auto i) {
                       return locals[i].render(w);
                   })
                .and_then([&] { return code::render_expr(expr, w); });
        }
    };

    constexpr auto render_with_payload_length(binary::writer& w, auto&& render) {
        auto buffer = w.remain();
        auto offset = w.offset();
        return render(w).and_then([&]() -> result<void> {
            auto len = w.offset() - offset;
            if (len > ~std::uint32_t(0)) {
                return unexpect(Error::large_output);
            }
            auto len_len = binary::len_leb128_uint(len);
            if (w.remain().size() < len_len) {
                return unexpect(Error::short_buffer);
            }
            view::shift(buffer.substr(0, len_len + len), len_len, 0, len);
            binary::writer tmpw{buffer.substr(0, len_len)};
            return render_uint(tmpw, std::uint32_t(len)).transform([&] {
                assert(tmpw.full());
                w.offset(len_len);
            });
        });
    }

    struct Code {
        std::uint32_t len = 0;
        Func func;

        auto parse(binary::reader& r) {
            return parse_uint(r, len).and_then([&] {
                binary::reader s{r.remain().substr(0, len)};
                return func.parse(s).and_then([&]() -> result<void> {
                                        if (!s.empty()) {
                                            return unexpect(Error::large_input);
                                        }
                                        return {};
                                    })
                    .transform([&] { r.offset(len); });
            });
        }

        auto render(binary::writer& w) const {
            return render_with_payload_length(w, [&](binary::writer& w) {
                return func.render(w);
            });
        }
    };

    struct Codes {
        std::vector<Code> codes;

        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Code>(r, codes);
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, codes.size(), [&](auto i) {
                return codes[i].render(w);
            });
        }
    };

    struct Data0 {
        code::Expr expr;
        view::rvec data;
        auto parse(binary::reader& r) {
            return code::parse_expr(r)
                .transform(assign_to(expr))
                .and_then([&] { return parse_byte_vec(r, data); });
        }

        auto render(binary::writer& w) const {
            return code::render_expr(expr, w)
                .and_then([&] { return render_byte_vec(w, data); });
        }
    };

    struct Data1 {
        view::rvec data;

        constexpr auto parse(binary::reader& r) {
            return parse_byte_vec(r, data);
        }

        constexpr auto render(binary::writer& w) const {
            return render_byte_vec(w, data);
        }
    };

    struct Data2 {
        Index mem_index = 0;
        code::Expr expr;
        view::rvec data;

        auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r, mem_index)
                .and_then([&] { return code::parse_expr(r); })
                .transform(assign_to(expr))
                .and_then([&] { return parse_byte_vec(r, data); });
        }

        auto render(binary::writer& w) const {
            return render_uint(w, mem_index)
                .and_then([&] { return code::render_expr(expr, w); })
                .and_then([&] { return render_byte_vec(w, data); });
        }
    };

    struct Data {
        using DataV = std::variant<Data0, Data1, Data2>;
        DataV data;

        constexpr auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r).and_then([&](std::uint32_t i) -> result<void> {
                switch (i) {
                    default:
                        return unexpect(Error::unexpected_instruction);
                    case 0:
                        data = DataV{std::in_place_type<Data0>};
                        break;
                    case 1:
                        data = DataV{std::in_place_type<Data1>};
                        break;
                    case 2:
                        data = DataV{std::in_place_type<Data2>};
                        break;
                }
                return std::visit([&](auto&& data) -> result<void> {
                    return data.parse(r);
                },
                                  data);
            });
        }

        constexpr auto render(binary::writer& w) const {
            return render_uint(w, std::uint32_t(data.index())).and_then([&] {
                return std::visit([&](auto&& data) -> result<void> {
                    return data.render(w);
                },
                                  data);
            });
        }
    };

    struct DataList {
        std::vector<Data> data;

        auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Data>(r, data);
            });
        }

        auto render(binary::writer& w) const {
            return render_vec(w, data.size(), [&](auto i) {
                return data[i].render(w);
            });
        }
    };

    struct DataCount {
        std::uint32_t count = 0;

        constexpr auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r, count);
        }

        constexpr auto render(binary::writer& w) const {
            return render_uint(w, count);
        }
    };

    struct Section {
        SectionHeader hdr;
        using SectionV =
            std::variant<Custom, Type, Imports, Function,
                         Table, Memory, Globals, Exports,
                         Start, Elements, Codes, DataList,
                         DataCount, Unspec>;
        SectionV body;

        constexpr auto parse(binary::reader& r) {
            return hdr.parse(r).and_then([&]() -> result<void> {
                switch (hdr.id) {
                    case ID::custom:
                        body = SectionV{std::in_place_type<Custom>};
                        break;
                    case ID::type:
                        body = SectionV{std::in_place_type<Type>};
                        break;
                    case ID::import_:
                        body = SectionV{std::in_place_type<Imports>};
                        break;
                    case ID::function:
                        body = SectionV{std::in_place_type<Function>};
                        break;
                    case ID::table:
                        body = SectionV{std::in_place_type<Table>};
                        break;
                    case ID::memory:
                        body = SectionV{std::in_place_type<Memory>};
                        break;
                    case ID::global:
                        body = SectionV{std::in_place_type<Globals>};
                        break;
                    case ID::export_:
                        body = SectionV{std::in_place_type<Exports>};
                        break;
                    case ID::start:
                        body = SectionV{std::in_place_type<Start>};
                        break;
                    case ID::element:
                        body = SectionV{std::in_place_type<Elements>};
                        break;
                    case ID::code:
                        body = SectionV{std::in_place_type<Codes>};
                        break;
                    case ID::data:
                        body = SectionV{std::in_place_type<DataList>};
                        break;
                    case ID::data_count:
                        body = SectionV{std::in_place_type<DataCount>};
                        break;
                    default:
                        body = SectionV{std::in_place_type<Unspec>};
                        break;
                }
                auto [data, ok] = r.read(hdr.len);
                if (!ok) {
                    return unexpect(Error::short_input);
                }
                binary::reader s{data};
                auto res = std::visit(
                    [&](auto&& obj) {
                        return obj.parse(s);
                    },
                    body);
                if (!res) {
                    return res;
                }
                if (!s.empty()) {
                    return unexpect(Error::large_input);
                }
                return {};
            });
        }

        constexpr auto render(binary::writer& w) const {
            return write_byte(w, std::holds_alternative<Unspec>(body)
                                     ? byte(hdr.id)
                                     : byte(ID(body.index())))
                .and_then([&] {
                    return render_with_payload_length(w, [&](binary::writer& w) {
                        return std::visit([&](auto&& obj) {
                            return obj.render(w);
                        },
                                          body);
                    });
                });
        }
    };
}  // namespace utils::wasm::section
