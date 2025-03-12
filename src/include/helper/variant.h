/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>
#include <type_traits>
#include <memory>
#include <core/byte.h>

namespace futils::helper::alt {

    template <typename T>
    constexpr size_t variant_size() {
        return sizeof(T);
    }

    template <typename T, class T2, typename... Ts>
    constexpr size_t variant_size() {
        constexpr auto next = variant_size<T2, Ts...>();
        return sizeof(T) > next ? sizeof(T) : next;
    }

    template <class T>
    constexpr size_t variant_align() {
        return alignof(T);
    }

    template <class T, class T2, class... Ts>
    constexpr size_t variant_align() {
        constexpr auto next = variant_align<T2, Ts...>();
        return alignof(T) > next ? alignof(T) : next;
    }

    template <typename Target>
    constexpr bool is_one_of() {
        return false;
    }

    template <typename Target, typename T, typename... Ts>
    constexpr bool is_one_of() {
        return std::is_same_v<Target, T> || is_one_of<Target, Ts...>();
    }

    template <typename Target>
    constexpr size_t index_of(size_t index) {
        return index;
    }

    template <typename Target, typename T, typename... Ts>
    constexpr size_t index_of(size_t index) {
        return std::is_same_v<Target, T> ? index : index_of<Target, Ts...>(index + 1);
    }

    template <size_t i>
    constexpr auto type_at() {
    }

    template <size_t i, typename T, typename... Ts>
    constexpr auto type_at() {
        if constexpr (i == 0) {
            return std::decay_t<T>();
        }
        else {
            return type_at<i - 1, Ts...>();
        }
    }

    enum class storage_op_t {
        move,
        copy,
        destroy,
        index,
    };

    template <class Target, class... Ts>
    concept is_one_of_v = is_one_of<Target, Ts...>();

    template <class T, class... Ts>
    struct variant_storage {
       public:
        constexpr static size_t size = variant_size<T, Ts...>();
        constexpr static size_t align = variant_align<T, Ts...>();
        constexpr static size_t type_count = sizeof...(Ts) + 1;

       private:
        alignas(align) futils::byte data_[size];
        std::uintptr_t (*storage_op)(storage_op_t, void*, void*) = nullptr;

        template <typename U>
        static std::uintptr_t storage_op_impl(storage_op_t op, void* dest, void* src) {
            constexpr size_t index = index_of<U, T, Ts...>(0);
            static_assert(index < type_count);
            switch (op) {
                case storage_op_t::move:
                    new (dest) U(std::move(*static_cast<U*>(src)));
                    break;
                case storage_op_t::copy:
                    new (dest) U(*static_cast<U*>(src));
                    break;
                case storage_op_t::destroy:
                    std::destroy_at(static_cast<U*>(dest));
                    break;
                case storage_op_t::index:
                    return index;
            }
            return 0;
        }

       public:
        template <class U>
            requires is_one_of_v<U, T, Ts...>
        constexpr variant_storage(U&& value)
            : storage_op(&storage_op_impl<std::decay_t<U>>) {
            std::construct_at<U>(std::launder(static_cast<U*>(static_cast<void*>(data_))), std::forward<U>(value));
        }

        constexpr variant_storage()
            requires std::is_default_constructible_v<T>
            : variant_storage(T{}) {}

        constexpr variant_storage(const variant_storage& other)
            : storage_op(other.storage_op) {
            storage_op(storage_op_t::copy, data_, other.data_);
        }

        constexpr variant_storage(variant_storage&& other)
            : storage_op(other.storage_op) {
            storage_op(storage_op_t::move, data_, other.data_);
        }

        constexpr variant_storage& operator=(const variant_storage& other) {
            if (this == &other) {
                return *this;
            }
            storage_op(storage_op_t::destroy, data_, nullptr);
            storage_op = other.storage_op;
            storage_op(storage_op_t::copy, data_, other.data_);
            return *this;
        }

        constexpr variant_storage& operator=(variant_storage&& other) {
            if (this == &other) {
                return *this;
            }
            storage_op(storage_op_t::destroy, data_, nullptr);
            storage_op = other.storage_op;
            storage_op(storage_op_t::move, data_, other.data_);
            return *this;
        }

        constexpr ~variant_storage() {
            storage_op(storage_op_t::destroy, data_, nullptr);
        }

        constexpr size_t index() const {
            return storage_op(storage_op_t::index, nullptr, nullptr);
        }

        template <class U>
        constexpr const U* get() const {
            constexpr auto idx = index_of<U, T, Ts...>(0);
            static_assert(idx < type_count, "type not in variant");
            return index() == idx ? static_cast<const U*>(static_cast<const void*>(data_)) : nullptr;
        }

        template <class U>
        constexpr U* get() {
            return const_cast<U*>(const_cast<const variant_storage*>(this)->get<U>());
        }

        template <size_t i>
        constexpr auto get() const {
            return get<type_at<i, T, Ts...>()>();
        }

        template <size_t i>
        constexpr auto get() {
            return get<type_at<i, T, Ts...>()>();
        }
    };

    namespace test {
        // 200個の構造体定義（テスト用）
        struct union_struct_1 {
            int dummy;
        };
        struct union_struct_2 {
            int dummy;
        };
        struct union_struct_3 {
            int dummy;
        };
        struct union_struct_4 {
            int dummy;
        };
        struct union_struct_5 {
            int dummy;
        };
        struct union_struct_6 {
            int dummy;
        };
        struct union_struct_7 {
            int dummy;
        };
        struct union_struct_8 {
            int dummy;
        };
        struct union_struct_9 {
            int dummy;
        };
        struct union_struct_10 {
            int dummy;
        };
        struct union_struct_11 {
            int dummy;
        };
        struct union_struct_12 {
            int dummy;
        };
        struct union_struct_13 {
            int dummy;
        };
        struct union_struct_14 {
            int dummy;
        };
        struct union_struct_15 {
            int dummy;
        };
        struct union_struct_16 {
            int dummy;
        };
        struct union_struct_17 {
            int dummy;
        };
        struct union_struct_18 {
            int dummy;
        };
        struct union_struct_19 {
            int dummy;
        };
        struct union_struct_20 {
            int dummy;
        };
        struct union_struct_21 {
            int dummy;
        };
        struct union_struct_22 {
            int dummy;
        };
        struct union_struct_23 {
            int dummy;
        };
        struct union_struct_24 {
            int dummy;
        };
        struct union_struct_25 {
            int dummy;
        };
        struct union_struct_26 {
            int dummy;
        };
        struct union_struct_27 {
            int dummy;
        };
        struct union_struct_28 {
            int dummy;
        };
        struct union_struct_29 {
            int dummy;
        };
        struct union_struct_30 {
            int dummy;
        };
        struct union_struct_31 {
            int dummy;
        };
        struct union_struct_32 {
            int dummy;
        };
        struct union_struct_33 {
            int dummy;
        };
        struct union_struct_34 {
            int dummy;
        };
        struct union_struct_35 {
            int dummy;
        };
        struct union_struct_36 {
            int dummy;
        };
        struct union_struct_37 {
            int dummy;
        };
        struct union_struct_38 {
            int dummy;
        };
        struct union_struct_39 {
            int dummy;
        };
        struct union_struct_40 {
            int dummy;
        };
        struct union_struct_41 {
            int dummy;
        };
        struct union_struct_42 {
            int dummy;
        };
        struct union_struct_43 {
            int dummy;
        };
        struct union_struct_44 {
            int dummy;
        };
        struct union_struct_45 {
            int dummy;
        };
        struct union_struct_46 {
            int dummy;
        };
        struct union_struct_47 {
            int dummy;
        };
        struct union_struct_48 {
            int dummy;
        };
        struct union_struct_49 {
            int dummy;
        };
        struct union_struct_50 {
            int dummy;
        };
        struct union_struct_51 {
            int dummy;
        };
        struct union_struct_52 {
            int dummy;
        };
        struct union_struct_53 {
            int dummy;
        };
        struct union_struct_54 {
            int dummy;
        };
        struct union_struct_55 {
            int dummy;
        };
        struct union_struct_56 {
            int dummy;
        };
        struct union_struct_57 {
            int dummy;
        };
        struct union_struct_58 {
            int dummy;
        };
        struct union_struct_59 {
            int dummy;
        };
        struct union_struct_60 {
            int dummy;
        };
        struct union_struct_61 {
            int dummy;
        };
        struct union_struct_62 {
            int dummy;
        };
        struct union_struct_63 {
            int dummy;
        };
        struct union_struct_64 {
            int dummy;
        };
        struct union_struct_65 {
            int dummy;
        };
        struct union_struct_66 {
            int dummy;
        };
        struct union_struct_67 {
            int dummy;
        };
        struct union_struct_68 {
            int dummy;
        };
        struct union_struct_69 {
            int dummy;
        };
        struct union_struct_70 {
            int dummy;
        };
        struct union_struct_71 {
            int dummy;
        };
        struct union_struct_72 {
            int dummy;
        };
        struct union_struct_73 {
            int dummy;
        };
        struct union_struct_74 {
            int dummy;
        };
        struct union_struct_75 {
            int dummy;
        };
        struct union_struct_76 {
            int dummy;
        };
        struct union_struct_77 {
            int dummy;
        };
        struct union_struct_78 {
            int dummy;
        };
        struct union_struct_79 {
            int dummy;
        };
        struct union_struct_80 {
            int dummy;
        };
        struct union_struct_81 {
            int dummy;
        };
        struct union_struct_82 {
            int dummy;
        };
        struct union_struct_83 {
            int dummy;
        };
        struct union_struct_84 {
            int dummy;
        };
        struct union_struct_85 {
            int dummy;
        };
        struct union_struct_86 {
            int dummy;
        };
        struct union_struct_87 {
            int dummy;
        };
        struct union_struct_88 {
            int dummy;
        };
        struct union_struct_89 {
            int dummy;
        };
        struct union_struct_90 {
            int dummy;
        };
        struct union_struct_91 {
            int dummy;
        };
        struct union_struct_92 {
            int dummy;
        };
        struct union_struct_93 {
            int dummy;
        };
        struct union_struct_94 {
            int dummy;
        };
        struct union_struct_95 {
            int dummy;
        };
        struct union_struct_96 {
            int dummy;
        };
        struct union_struct_97 {
            int dummy;
        };
        struct union_struct_98 {
            int dummy;
        };
        struct union_struct_99 {
            int dummy;
        };
        struct union_struct_100 {
            int dummy;
        };
        struct union_struct_101 {
            int dummy;
        };
        struct union_struct_102 {
            int dummy;
        };
        struct union_struct_103 {
            int dummy;
        };
        struct union_struct_104 {
            int dummy;
        };
        struct union_struct_105 {
            int dummy;
        };
        struct union_struct_106 {
            int dummy;
        };
        struct union_struct_107 {
            int dummy;
        };
        struct union_struct_108 {
            int dummy;
        };
        struct union_struct_109 {
            int dummy;
        };
        struct union_struct_110 {
            int dummy;
        };
        struct union_struct_111 {
            int dummy;
        };
        struct union_struct_112 {
            int dummy;
        };
        struct union_struct_113 {
            int dummy;
        };
        struct union_struct_114 {
            int dummy;
        };
        struct union_struct_115 {
            int dummy;
        };
        struct union_struct_116 {
            int dummy;
        };
        struct union_struct_117 {
            int dummy;
        };
        struct union_struct_118 {
            int dummy;
        };
        struct union_struct_119 {
            int dummy;
        };
        struct union_struct_120 {
            int dummy;
        };
        struct union_struct_121 {
            int dummy;
        };
        struct union_struct_122 {
            int dummy;
        };
        struct union_struct_123 {
            int dummy;
        };
        struct union_struct_124 {
            int dummy;
        };
        struct union_struct_125 {
            int dummy;
        };
        struct union_struct_126 {
            int dummy;
        };
        struct union_struct_127 {
            int dummy;
        };
        struct union_struct_128 {
            int dummy;
        };
        struct union_struct_129 {
            int dummy;
        };
        struct union_struct_130 {
            int dummy;
        };
        struct union_struct_131 {
            int dummy;
        };
        struct union_struct_132 {
            int dummy;
        };
        struct union_struct_133 {
            int dummy;
        };
        struct union_struct_134 {
            int dummy;
        };
        struct union_struct_135 {
            int dummy;
        };
        struct union_struct_136 {
            int dummy;
        };
        struct union_struct_137 {
            int dummy;
        };
        struct union_struct_138 {
            int dummy;
        };
        struct union_struct_139 {
            int dummy;
        };
        struct union_struct_140 {
            int dummy;
        };
        struct union_struct_141 {
            int dummy;
        };
        struct union_struct_142 {
            int dummy;
        };
        struct union_struct_143 {
            int dummy;
        };
        struct union_struct_144 {
            int dummy;
        };
        struct union_struct_145 {
            int dummy;
        };
        struct union_struct_146 {
            int dummy;
        };
        struct union_struct_147 {
            int dummy;
        };
        struct union_struct_148 {
            int dummy;
        };
        struct union_struct_149 {
            int dummy;
        };
        struct union_struct_150 {
            int dummy;
        };
        struct union_struct_151 {
            int dummy;
        };
        struct union_struct_152 {
            int dummy;
        };
        struct union_struct_153 {
            int dummy;
        };
        struct union_struct_154 {
            int dummy;
        };
        struct union_struct_155 {
            int dummy;
        };
        struct union_struct_156 {
            int dummy;
        };
        struct union_struct_157 {
            int dummy;
        };
        struct union_struct_158 {
            int dummy;
        };
        struct union_struct_159 {
            int dummy;
        };
        struct union_struct_160 {
            int dummy;
        };
        struct union_struct_161 {
            int dummy;
        };
        struct union_struct_162 {
            int dummy;
        };
        struct union_struct_163 {
            int dummy;
        };
        struct union_struct_164 {
            int dummy;
        };
        struct union_struct_165 {
            int dummy;
        };
        struct union_struct_166 {
            int dummy;
        };
        struct union_struct_167 {
            int dummy;
        };
        struct union_struct_168 {
            int dummy;
        };
        struct union_struct_169 {
            int dummy;
        };
        struct union_struct_170 {
            int dummy;
        };
        struct union_struct_171 {
            int dummy;
        };
        struct union_struct_172 {
            int dummy;
        };
        struct union_struct_173 {
            int dummy;
        };
        struct union_struct_174 {
            int dummy;
        };
        struct union_struct_175 {
            int dummy;
        };
        struct union_struct_176 {
            int dummy;
        };
        struct union_struct_177 {
            int dummy;
        };
        struct union_struct_178 {
            int dummy;
        };
        struct union_struct_179 {
            int dummy;
        };
        struct union_struct_180 {
            int dummy;
        };
        struct union_struct_181 {
            int dummy;
        };
        struct union_struct_182 {
            int dummy;
        };
        struct union_struct_183 {
            int dummy;
        };
        struct union_struct_184 {
            int dummy;
        };
        struct union_struct_185 {
            int dummy;
        };
        struct union_struct_186 {
            int dummy;
        };
        struct union_struct_187 {
            int dummy;
        };
        struct union_struct_188 {
            int dummy;
        };
        struct union_struct_189 {
            int dummy;
        };
        struct union_struct_190 {
            int dummy;
        };
        struct union_struct_191 {
            int dummy;
        };
        struct union_struct_192 {
            int dummy;
        };
        struct union_struct_193 {
            int dummy;
        };
        struct union_struct_194 {
            int dummy;
        };
        struct union_struct_195 {
            int dummy;
        };
        struct union_struct_196 {
            int dummy;
        };
        struct union_struct_197 {
            int dummy;
        };
        struct union_struct_198 {
            int dummy;
        };
        struct union_struct_199 {
            int dummy;
        };
        struct union_struct_200 {
            int dummy;
            size_t dummy2;
        };

// 200個の構造体をコンマで繋げたマクロ
#define MANY_TYPES                                                                                                                                                                          \
    union_struct_1, union_struct_2, union_struct_3, union_struct_4, union_struct_5, union_struct_6, union_struct_7, union_struct_8, union_struct_9, union_struct_10,                        \
        union_struct_11, union_struct_12, union_struct_13, union_struct_14, union_struct_15, union_struct_16, union_struct_17, union_struct_18, union_struct_19, union_struct_20,           \
        union_struct_21, union_struct_22, union_struct_23, union_struct_24, union_struct_25, union_struct_26, union_struct_27, union_struct_28, union_struct_29, union_struct_30,           \
        union_struct_31, union_struct_32, union_struct_33, union_struct_34, union_struct_35, union_struct_36, union_struct_37, union_struct_38, union_struct_39, union_struct_40,           \
        union_struct_41, union_struct_42, union_struct_43, union_struct_44, union_struct_45, union_struct_46, union_struct_47, union_struct_48, union_struct_49, union_struct_50,           \
        union_struct_51, union_struct_52, union_struct_53, union_struct_54, union_struct_55, union_struct_56, union_struct_57, union_struct_58, union_struct_59, union_struct_60,           \
        union_struct_61, union_struct_62, union_struct_63, union_struct_64, union_struct_65, union_struct_66, union_struct_67, union_struct_68, union_struct_69, union_struct_70,           \
        union_struct_71, union_struct_72, union_struct_73, union_struct_74, union_struct_75, union_struct_76, union_struct_77, union_struct_78, union_struct_79, union_struct_80,           \
        union_struct_81, union_struct_82, union_struct_83, union_struct_84, union_struct_85, union_struct_86, union_struct_87, union_struct_88, union_struct_89, union_struct_90,           \
        union_struct_91, union_struct_92, union_struct_93, union_struct_94, union_struct_95, union_struct_96, union_struct_97, union_struct_98, union_struct_99, union_struct_100,          \
        union_struct_101, union_struct_102, union_struct_103, union_struct_104, union_struct_105, union_struct_106, union_struct_107, union_struct_108, union_struct_109, union_struct_110, \
        union_struct_111, union_struct_112, union_struct_113, union_struct_114, union_struct_115, union_struct_116, union_struct_117, union_struct_118, union_struct_119, union_struct_120, \
        union_struct_121, union_struct_122, union_struct_123, union_struct_124, union_struct_125, union_struct_126, union_struct_127, union_struct_128, union_struct_129, union_struct_130, \
        union_struct_131, union_struct_132, union_struct_133, union_struct_134, union_struct_135, union_struct_136, union_struct_137, union_struct_138, union_struct_139, union_struct_140, \
        union_struct_141, union_struct_142, union_struct_143, union_struct_144, union_struct_145, union_struct_146, union_struct_147, union_struct_148, union_struct_149, union_struct_150, \
        union_struct_151, union_struct_152, union_struct_153, union_struct_154, union_struct_155, union_struct_156, union_struct_157, union_struct_158, union_struct_159, union_struct_160, \
        union_struct_161, union_struct_162, union_struct_163, union_struct_164, union_struct_165, union_struct_166, union_struct_167, union_struct_168, union_struct_169, union_struct_170, \
        union_struct_171, union_struct_172, union_struct_173, union_struct_174, union_struct_175, union_struct_176, union_struct_177, union_struct_178, union_struct_179, union_struct_180, \
        union_struct_181, union_struct_182, union_struct_183, union_struct_184, union_struct_185, union_struct_186, union_struct_187, union_struct_188, union_struct_189, union_struct_190, \
        union_struct_191, union_struct_192, union_struct_193, union_struct_194, union_struct_195, union_struct_196, union_struct_197, union_struct_198, union_struct_199, union_struct_200

        using target_type = union_struct_200;
        constexpr size_t target_index = 199;

        constexpr bool many_of_size() {
            constexpr auto size = variant_size<MANY_TYPES>();
            return size == sizeof(target_type);
        }
        static_assert(many_of_size());
        constexpr bool many_of_align() {
            constexpr auto align = variant_align<MANY_TYPES>();
            return align == alignof(target_type);
        }
        static_assert(many_of_align());

        constexpr bool many_of_one_of() {
            return is_one_of<target_type, MANY_TYPES>();
        }
        static_assert(many_of_one_of());

        constexpr bool many_of_index_of() {
            constexpr auto index = index_of<target_type, MANY_TYPES>(0);
            return index == 199;
        }

        static_assert(many_of_index_of());

        constexpr bool many_of_type_at() {
            auto type = type_at<target_index, MANY_TYPES>();
            return std::is_same_v<target_type, decltype(type)>;
        }

        static_assert(many_of_type_at());

        constexpr bool many_of_storage() {
            variant_storage<MANY_TYPES> storage(target_type{});
            return storage.index() == target_index;
        }

        // static_assert(many_of_storage());

    }  // namespace test

}  // namespace futils::helper::alt

namespace std {

}