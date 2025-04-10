/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// buffer -buffer traits
#pragma once

#include <type_traits>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <core/strlen.h>
#include <helper/disable_self.h>

namespace futils {

    template <class Seq, bool is_ptr = std::is_pointer_v<std::decay_t<Seq>>>
    struct Buffer {
        using raw_type = Seq;
        using unref_type = std::remove_reference_t<Seq>;
        using char_type = decltype(std::declval<Seq>()[0]);

        using unconst_type = std::remove_const_t<unref_type>;

        raw_type buffer;

        constexpr Buffer() {}

        template <class T, helper_disable_self(Buffer, T)>
        constexpr Buffer(T&& in)
            : buffer(std::forward<T>(in)) {}

        constexpr Buffer(Buffer&& in)
            requires(std::is_move_constructible_v<Seq>)
            : buffer(std::forward<decltype(in.buffer)>(in.buffer)) {}

        constexpr decltype(auto) at(size_t position) {
            return buffer[position];
        }

        constexpr decltype(auto) at(size_t position) const {
            return buffer[position];
        }

        constexpr size_t size() const {
            return buffer.size();
        }

        constexpr bool in_range(size_t position) const {
            return size() > position;
        }
    };

    template <class C>
    struct Buffer<C*, true> {
        using raw_type = C*;
        using unref_type = C*;
        using char_type = C&;

        using unconst_type = std::remove_const_t<unref_type>;

        C* buffer = nullptr;
        size_t size_ = 0;

        constexpr Buffer() {}
        constexpr Buffer(C* ptr)
            : buffer(ptr), size_(ptr ? strlen(ptr) : 0) {}

        constexpr Buffer(C* ptr, size_t size)
            : buffer(ptr), size_(ptr ? size : 0) {}

        constexpr Buffer(Buffer&& in)
            : buffer(std::forward<unref_type>(in.buffer)), size_(in.size_) {}

        constexpr char_type at(size_t position) {
            return buffer[position];
        }

        constexpr char_type at(size_t position) const {
            return buffer[position];
        }

        constexpr size_t size() const {
            return size_;
        }

        constexpr bool in_range(size_t position) const {
            return size_ > position;
        }
    };

    template <class T, bool = std::is_pointer_v<std::decay_t<T>>>
    struct BufferType {
        using type = T;
        using char_type = typename Buffer<type>::char_type;
    };

    template <class T>
    struct BufferType<T, true> {
        using type = std::decay_t<T>;
        using char_type = typename Buffer<type>::char_type;
    };

    template <class T>
    using buffer_t = typename BufferType<T>::type;

    template <class T>
    constexpr size_t bufsize(T&& t) {
        Buffer<buffer_t<T&>> f(t);
        return f.size();
    }

}  // namespace futils
