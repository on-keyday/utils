/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../binary/number.h"
#include "../wrap/light/enum.h"
#include "../fnet/quic/varint.h"
#include "../binary/term.h"

namespace utils {
    namespace reflect::layout {
        enum class LayoutFlag : byte {
            flag_none = 0x00,
            flag_int = 0x00,     // flag|interger length
            flag_uint = 0x01,    // flag|integer length
            flag_float = 0x02,   // flag|floating point length
            flag_array = 0x03,   // flag|array element count|element layout
            flag_struct = 0x04,  // flag|struct element count|each element layout...
            flag_union = 0x05,   // flag|union element count|each element layout...
            flag_enum = 0x06,    // flag|enum base integer length
            base_flags = 0x07,
            flag_ptr = 0x08,          // flag only
            flag_ptr_element = 0x80,  // has ptr element type info
            flag_const = 0x10,        // const
            // meta data is inserted after flag field if below flag is enabled
            // null terminated
            // order is class_name -> field_name
            flag_meta_field = 0x20,  // meta data (field name)
            flag_meta_type = 0x40,   // meta data (class name)
        };

        DEFINE_ENUM_FLAGOP(LayoutFlag)

        constexpr bool is_base(LayoutFlag t, LayoutFlag f) {
            return (t & LayoutFlag::base_flags) == f;
        }

        struct LayoutHeader {
            LayoutFlag flag;
            view::rvec meta_class;
            view::rvec meta_field;
            size_t length = 0;

            constexpr bool is_ptr() const {
                return any(flag & LayoutFlag::flag_ptr);
            }

            constexpr std::uint64_t is_int() const {
                return is_base(flag, LayoutFlag::flag_int) ? length : 0;
            }

            constexpr std::uint64_t is_uint() const {
                return is_base(flag, LayoutFlag::flag_uint) ? length : 0;
            }

            constexpr std::uint64_t is_float() const {
                return is_base(flag, LayoutFlag::flag_float) ? length : 0;
            }

            constexpr std::uint64_t is_enum() const {
                return is_base(flag, LayoutFlag::flag_enum) ? length : 0;
            }

            constexpr std::uint64_t is_struct() const {
                return is_base(flag, LayoutFlag::flag_struct) ? length : 0;
            }

            constexpr bool is_empty_struct() const {
                return is_base(flag, LayoutFlag::flag_struct) && length == 0;
            }

            constexpr std::uint64_t is_array() const {
                return is_base(flag, LayoutFlag::flag_array) ? length : 0;
            }
            constexpr bool is_empty_array() const {
                return is_base(flag, LayoutFlag::flag_array) && length == 0;
            }

            constexpr std::uint64_t is_union() const {
                return is_base(flag, LayoutFlag::flag_union) ? length : 0;
            }

            constexpr bool is_empty_union() const {
                return is_base(flag, LayoutFlag::flag_union) && length == 0;
            }

            constexpr bool write(binary::writer& layout) const {
                if (!binary::write_num(layout, flag)) {
                    return false;
                }
                if (any(flag & LayoutFlag::flag_meta_type)) {
                    if (!binary::write_terminated(layout, meta_class)) {
                        return false;
                    }
                }
                if (any(flag & LayoutFlag::flag_meta_field)) {
                    if (!binary::write_terminated(layout, meta_field)) {
                        return false;
                    }
                }
                if (any(flag & LayoutFlag::flag_ptr)) {
                    return true;
                }
                return fnet::quic::varint::write(layout, length);
            }

            constexpr bool read(binary::reader& layout) {
                if (layout.empty()) {
                    return false;
                }
                flag = LayoutFlag(layout.top());
                layout.offset(1);
                if (any(flag & LayoutFlag::flag_meta_type)) {
                    if (!binary::read_terminated(layout, meta_class)) {
                        return false;
                    }
                }
                if (any(flag & LayoutFlag::flag_meta_field)) {
                    if (!binary::read_terminated(layout, meta_field)) {
                        return false;
                    }
                }
                if (any(flag & LayoutFlag::flag_ptr)) {
                    length = 0;
                    return true;
                }
                auto [len, ok] = fnet::quic::varint::read(layout);
                if (!ok) {
                    return false;
                }
                length = len;
                return true;
            }
        };

        constexpr LayoutFlag metadata(view::rvec type, view::rvec field) {
            return (type.size() ? LayoutFlag::flag_meta_type : LayoutFlag::flag_none) |
                   (field.size() ? LayoutFlag::flag_meta_field : LayoutFlag::flag_none);
        }

        struct Metadata {
            const char* type = nullptr;
            const char* field = nullptr;
        };

        template <class T>
        struct NumberLayout : Metadata {
            static_assert(std::is_integral_v<T> ||
                          std::is_floating_point_v<T> ||
                          std::is_enum_v<T>);
            constexpr bool write(binary::writer& w) const {
                LayoutHeader head;
                if constexpr (std::is_floating_point_v<T>) {
                    head.flag = LayoutFlag::flag_float;
                }
                else if constexpr (std::is_enum_v<T>) {
                    head.flag = LayoutFlag::flag_enum;
                }
                else {
                    head.flag = std::is_unsigned_v<T> ? LayoutFlag::flag_uint : LayoutFlag::flag_int;
                }
                head.flag |= metadata(type, field);
                head.meta_class = type;
                head.meta_field = field;
                head.length = sizeof(T);
                return head.write(w);
            }
        };

        template <LayoutFlag base, class... Field>
        struct StructLayoutBase : Metadata {
            std::tuple<Field...> fields;

            constexpr bool write(binary::writer& w) const {
                LayoutHeader head;
                head.flag = base | metadata(type, field);
                head.meta_class = type;
                head.meta_field = field;
                head.length = sizeof...(Field);
                if (!head.write(w)) {
                    return false;
                }
                return std::apply(
                    [&](auto&&... fields) {
                        auto fold = [&](auto& field) {
                            return field.write(w);
                        };
                        return (... && fold(w));
                    },
                    fields);
            }
        };

        template <class... Field>
        struct StructLayout : StructLayoutBase<LayoutFlag::flag_struct, Field...> {};
        template <class... Field>
        struct UnionLayout : StructLayoutBase<LayoutFlag::flag_union, Field...> {};

        template <class Element>
        struct ArrayLayout : Metadata {
            size_t length;
            Element element;

            constexpr bool write(binary::writer& w) const {
                LayoutHeader head;
                head.flag = LayoutFlag::flag_array | metadata(type, field);
                head.meta_class = type;
                head.meta_field = field;
                head.length = length;
                if (!head.write(w)) {
                    return false;
                }
                return element.write(w);
            }
        };

        template <class InnerLayout>
        struct PointerLayout {};

        constexpr auto array(Metadata meta, size_t len, auto&& element) {
            return ArrayLayout<std::decay_t<decltype(element)>>{
                {meta.type, meta.filed},
                len,
                std::forward<decltype(element)>(element),
            };
        }

        constexpr auto struct_(Metadata meta, auto&&... fields) {
            return StructLayout<std::decay_t<decltype(fields)>>{
                {
                    {meta.type, meta.filed},
                    std::make_tuple(std::forward<decltype(fields)>(fields)...),
                },
            };
        }

        constexpr auto union_(Metadata meta, auto&&... fields) {
            return UnionLayout<std::decay_t<decltype(fields)>>{
                {
                    {meta.type, meta.filed},
                    std::make_tuple(std::forward<decltype(fields)>(fields)...),
                },
            };
        }

        template <class T>
        constexpr auto number(Metadata meta) {
            return NumberLayout<T>{
                {meta.type, meta.filed},
            };
        }

    }  // namespace reflect::layout
}  // namespace utils
