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

        constexpr result<void> render(binary::writer& w) {
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
    };

    struct Unspec {
        view::rvec data;

        constexpr result<void> parse(binary::reader& r) {
            r.read(data, r.remain().size());
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
        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Import>(r, imports);
            });
        }
    };

    struct Function {
        std::vector<Index> funcs;
        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_uint<std::uint32_t>(r).transform(push_back_to(funcs));
            });
        }
    };

    struct Table {
        std::vector<type::TableType> tables;
        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return type::parse_table_type(r).transform(push_back_to(tables));
            });
        }
    };

    struct Memory {
        std::vector<type::MemoryType> memories;
        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return type::parse_memory_type(r).transform(push_back_to(memories));
            });
        }
    };

    struct Global {
        type::GlobalType type;
        code::Expr expr;
        constexpr auto parse(binary::reader& r) {
            return type::parse_global_type(r)
                .transform(assign_to(type))
                .and_then([&] { return code::parse_expr(r); })
                .transform(assign_to(expr));
        }
    };

    struct Globals {
        std::vector<Global> globals;
        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Global>(r, globals);
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
    };

    struct Export {
        view::rvec name;
        ExportDesc desc;

        constexpr auto parse(binary::reader& r) {
            return parse_name(r, name).and_then([&] { return desc.parse(r); });
        }
    };

    struct Exports {
        std::vector<Export> exports;
        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Export>(r, exports);
            });
        }
    };

    struct Exprs {
        std::vector<code::Expr> exprs;

        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return code::parse_expr(r).transform(push_back_to(exprs));
            });
        }
    };

    struct Start {
        Index func = 0;
        constexpr auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r, func);
            type::parse_reftype(r);
        }
    };

    // Element0
    struct Element0 {
        code::Expr expr;
        Function funcs;

        constexpr auto parse(binary::reader& r) {
            return code::parse_expr(r)
                .and_then([&](code::Expr e) {
                    expr = std::move(e);
                    return funcs.parse(r);
                });
        }
    };

    // Element1
    struct Element1 {
        byte kind = 0;
        Function funcs;

        constexpr auto parse(binary::reader& r) {
            return read_byte(r)
                .and_then([&](byte k) {
                    kind = k;
                    return funcs.parse(r);
                });
        }
    };

    // Element2
    struct Element2 {
        Index table_index = 0;
        code::Expr expr;
        byte kind = 0;
        Function funcs;

        constexpr auto parse(binary::reader& r) {
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
    };

    // Element3
    struct Element3 {
        byte kind = 0;
        Function funcs;

        constexpr auto parse(binary::reader& r) {
            return read_byte(r)
                .and_then([&](byte k) {
                    kind = k;
                    return funcs.parse(r);
                });
        }
    };

    // Element4
    struct Element4 {
        code::Expr expr;
        Exprs exprs;

        constexpr auto parse(binary::reader& r) {
            return code::parse_expr(r)
                .and_then([&](code::Expr e) {
                    expr = std::move(e);
                    return exprs.parse(r);
                });
        }
    };

    // Element5
    struct Element5 {
        type::Type reftype;
        Exprs exprs;

        constexpr auto parse(binary::reader& r) {
            return type::parse_reftype(r)
                .and_then([&](type::Type t) {
                    reftype = std::move(t);
                    return exprs.parse(r);
                });
        }
    };

    // Element6
    struct Element6 {
        Index table_index = 0;
        code::Expr expr;
        type::Type reftype;
        Exprs exprs;

        constexpr auto parse(binary::reader& r) {
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
    };

    // Element7
    struct Element7 {
        type::Type reftype;
        Exprs exprs;

        constexpr auto parse(binary::reader& r) {
            return type::parse_reftype(r)
                .and_then([&](type::Type t) {
                    reftype = std::move(t);
                    return exprs.parse(r);
                });
        }
    };

    struct Element {
        std::variant<Element0, Element1, Element2, Element3, Element4, Element5, Element6, Element7> element;
        constexpr auto parse(binary::reader& r) {
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
    };

    struct Elements {
        std::vector<Element> elements;
        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Element>(r, elements);
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
    };

    struct Func {
        std::vector<Locals> locals;
        code::Expr expr;

        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                       return parse_vec_elm<Locals>(r, locals);
                   })
                .and_then([&] { return code::parse_expr(r); })
                .transform(assign_to(expr));
        }
    };

    struct Code {
        std::uint32_t len = 0;
        Func func;

        constexpr auto parse(binary::reader& r) {
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
    };

    struct Codes {
        std::vector<Code> codes;

        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Code>(r, codes);
            });
        }
    };

    struct Data0 {
        code::Expr expr;
        view::rvec data;
        constexpr auto parse(binary::reader& r) {
            return code::parse_expr(r)
                .transform(assign_to(expr))
                .and_then([&] { return parse_byte_vec(r, data); });
        }
    };

    struct Data1 {
        view::rvec data;

        constexpr auto parse(binary::reader& r) {
            return parse_byte_vec(r, data);
        }
    };

    struct Data2 {
        Index mem_index = 0;
        code::Expr expr;
        view::rvec data;

        constexpr auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r, mem_index)
                .and_then([&] { return code::parse_expr(r); })
                .transform(assign_to(expr))
                .and_then([&] { return parse_byte_vec(r, data); });
        }
    };

    struct Data {
        std::variant<Data0, Data1, Data2> data;

        constexpr auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r).and_then([&](std::uint32_t i) -> result<void> {
                switch (i) {
                    default:
                        return unexpect(Error::unexpected_instruction);
                    case 0:
                        data = Data0{};
                        break;
                    case 1:
                        data = Data1{};
                        break;
                    case 2:
                        data = Data2{};
                        break;
                }
                return std::visit([&](auto&& data) {
                    return data.parse(r);
                },
                                  data);
            });
        }
    };

    struct DataList {
        std::vector<Data> data;

        constexpr auto parse(binary::reader& r) {
            return parse_vec(r, [&](auto i) {
                return parse_vec_elm<Data>(r, data);
            });
        }
    };

    struct DataCount {
        std::uint32_t count = 0;

        constexpr auto parse(binary::reader& r) {
            return parse_uint<std::uint32_t>(r, count);
        }
    };

    struct Section {
        SectionHeader hdr;
        std::variant<Unspec,
                     Custom, Type, Imports, Function,
                     Table, Memory, Globals, Exports,
                     Start, Elements, Codes, DataList,
                     DataCount>
            body;

        constexpr auto parse(binary::reader& r) {
            return hdr.parse(r).and_then([&]() -> result<void> {
                switch (hdr.id) {
                    case ID::custom:
                        body = Custom{};
                        break;
                    case ID::type:
                        body = Type{};
                        break;
                    case ID::import_:
                        body = Imports{};
                        break;
                    case ID::function:
                        body = Function{};
                        break;
                    case ID::table:
                        body = Table{};
                        break;
                    case ID::memory:
                        body = Memory{};
                        break;
                    case ID::global:
                        body = Globals{};
                        break;
                    case ID::export_:
                        body = Exports{};
                        break;
                    case ID::start:
                        body = Start{};
                        break;
                    case ID::element:
                        body = Elements{};
                        break;
                    case ID::code:
                        body = Codes{};
                        break;
                    case ID::data:
                        body = DataList{};
                        break;
                    case ID::data_count:
                        body = DataCount{};
                        break;
                    default:
                        body = Unspec{};
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
    };
}  // namespace utils::wasm::section
