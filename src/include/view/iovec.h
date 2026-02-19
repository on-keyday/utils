/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// iovec - io vector
#pragma once
#include <core/strlen.h>
#include <core/byte.h>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <iterator>
#include <compare>
#include <utility>
#include <helper/omit_empty.h>

namespace futils {
    namespace view {

        namespace internal {

            template <class T, size_t size>
            concept non_const_ptr = std::is_pointer_v<T> &&
                                    std::is_const_v<std::remove_pointer_t<T>> == false &&
                                    sizeof(std::remove_pointer_t<T>) == size;

            template <class T, size_t size>
            concept const_ptr = std::is_pointer_v<T> &&
                                sizeof(std::remove_pointer_t<T>) == size;

            template <class T, class C>
            concept is_not_const_data = requires(T t) {
                { std::data(t) } -> non_const_ptr<sizeof(C)>;
            };

            template <class T, class C>
            concept is_const_data = requires(T t) {
                { std::data(t) } -> const_ptr<sizeof(C)>;
            };

            template <class T>
            struct default_as_char_ {
                using U = char;
            };

            template <>
            struct default_as_char_<char> {
                using U = byte;
            };

            template <class T>
            using default_as_char = typename default_as_char_<T>::U;

            template <class C>
            constexpr auto ptrdiff_len(const C* begin, const C* end)
                -> decltype(end - begin) {
                if (!std::is_constant_evaluated()) {
                    if (begin > end) {
                        return 0;
                    }
                }
                auto diff = end - begin;
                return diff < 0 ? 0 : diff;
            }
        }  // namespace internal

        template <class C>
        struct basic_rvec {
           protected:
            const C* data_;
            size_t size_;

           public:
            constexpr basic_rvec() noexcept
                : data_(nullptr), size_(0) {}
            constexpr basic_rvec(const C* d, size_t s) noexcept
                : data_(d), size_(d ? s : 0) {}

            constexpr explicit basic_rvec(const C* d) noexcept
                : data_(d), size_(d ? futils::strlen(d) : 0) {}

            template <class End, std::enable_if_t<std::is_same_v<End, const C*> || std::is_same_v<End, C*>, int> = 0>
            constexpr basic_rvec(const C* begin, End end) noexcept
                : basic_rvec(begin, internal::ptrdiff_len(begin, end)) {}

#define rvec_enable_U template <class U, std::enable_if_t<!std::is_same_v<C, U> && sizeof(U) == sizeof(C), int> = 0>
            rvec_enable_U basic_rvec(const U* c, size_t s) noexcept
                : basic_rvec(reinterpret_cast<const C*>(c), s) {
                static_assert(!std::is_same_v<C, U> && sizeof(U) == sizeof(C));
            }

            rvec_enable_U
            basic_rvec(const U* c)
                : basic_rvec(reinterpret_cast<const C*>(c)) {
                static_assert(!std::is_same_v<C, U> && sizeof(U) == sizeof(C));
            }

            rvec_enable_U
            basic_rvec(const U* begin, const U* end)
                : basic_rvec(reinterpret_cast<const C*>(begin), reinterpret_cast<const C*>(end)) {
                static_assert(!std::is_same_v<C, U> && sizeof(U) == sizeof(C));
            }

            template <class T>
                requires internal::is_const_data<T, C>
            constexpr basic_rvec(T&& t) noexcept
                : basic_rvec(std::data(t), std::size(t)) {}

            constexpr const C& operator[](size_t i) const noexcept {
                return data_[i];
            }

            constexpr const C* begin() const noexcept {
                return data_;
            }

            constexpr const C* end() const noexcept {
                return data_ + size_;
            }

            constexpr const C* cbegin() const noexcept {
                return data_;
            }

            constexpr const C* cend() const noexcept {
                return data_ + size_;
            }

            constexpr const C* data() const noexcept {
                return data_;
            }

            constexpr const C* cdata() const noexcept {
                return data_;
            }

            template <class U = internal::default_as_char<C>>
            const U* as_char() const noexcept {
                return reinterpret_cast<const U*>(data_);
            }

            constexpr size_t size() const noexcept {
                return size_;
            }

            constexpr bool empty() const noexcept {
                return size_ == 0;
            }

            constexpr bool null() const noexcept {
                return data_ == nullptr;
            }

            constexpr basic_rvec rsubstr(size_t pos = 0, size_t n = ~0) const noexcept {
                if (pos > size_) {
                    return {};  // empty
                }
                if (n > size_ - pos) {
                    n = size_ - pos;
                }
                return basic_rvec(data_ + pos, n);
            }

            constexpr basic_rvec substr(size_t pos = 0, size_t n = ~0) const noexcept {
                return rsubstr(pos, n);
            }

            constexpr friend bool operator==(const basic_rvec& l, const basic_rvec& r) noexcept {
                if (l.size_ != r.size_) {
                    return false;
                }
                if (!std::is_constant_evaluated()) {
                    if (l.data_ == r.data_) {
                        return true;
                    }
                }
                for (size_t i = 0; i < l.size_; i++) {
                    if (l[i] != r[i]) {
                        return false;
                    }
                }
                return true;
            }

            constexpr int compare(const basic_rvec& r) const noexcept {
                size_t min_ = size_ < r.size_ ? size_ : r.size_;
                size_t i = 0;
                for (; i < min_; i++) {
                    if (data_[i] != r.data_[i]) {
                        break;
                    }
                }
                if (i == min_) {
                    return size_ == r.size_ ? 0 : size_ < r.size_ ? -1
                                                                  : 1;
                }
                return data_[i] < r.data_[i] ? -1 : 1;
            }

            constexpr friend std::strong_ordering operator<=>(const basic_rvec& l, const basic_rvec& r) {
                return l.compare(r) <=> 0;
            }

            constexpr const C& front() const noexcept {
                return data_[0];
            }

            constexpr const C& back() const noexcept {
                return data_[size_ - 1];
            }
        };

        template <class C>
        struct basic_wvec : public basic_rvec<C> {
            constexpr basic_wvec() noexcept = default;
            constexpr basic_wvec(C* d, size_t s) noexcept
                : basic_rvec<C>(d, s) {}

            template <class End, std::enable_if_t<std::is_same_v<End, C*>, int> = 0>
            constexpr basic_wvec(C* begin, End end) noexcept
                : basic_rvec<C>(begin, end) {}

            rvec_enable_U
            basic_wvec(U* d, size_t s) noexcept
                : basic_rvec<C>(d, s) {}

            rvec_enable_U
            basic_wvec(U* begin, U* end)
                : basic_rvec<C>(begin, end) {}

            template <internal::is_not_const_data<C> T>
            constexpr basic_wvec(T&& t) noexcept
                : basic_rvec<C>(t) {}

            constexpr C* data() noexcept {
                return const_cast<C*>(this->data_);
            }

            // because clang can't detect data in basic_rvec
            // redefine this
            constexpr const C* data() const noexcept {
                return this->data_;
            }

            constexpr C& operator[](size_t i) noexcept {
                return data()[i];
            }

            constexpr const C& operator[](size_t i) const noexcept {
                return data()[i];
            }

            constexpr C* begin() noexcept {
                return data();
            }

            constexpr C* end() noexcept {
                return data() + this->size_;
            }

            template <class U = internal::default_as_char<C>>
            U* as_char() noexcept {
                return reinterpret_cast<U*>(data());
            }

            constexpr basic_wvec wsubstr(size_t pos = 0, size_t n = ~0) const noexcept {
                if (pos > this->size_) {
                    return {};  // empty
                }
                if (n > this->size_ - pos) {
                    n = this->size_ - pos;
                }
                return basic_wvec(const_cast<C*>(this->data_) + pos, n);
            }

            constexpr basic_wvec substr(size_t pos = 0, size_t n = ~0) const noexcept {
                return wsubstr(pos, n);
            }

            constexpr void fill(C d) {
                for (C& c : *this) {
                    c = d;
                }
            }

            constexpr void force_fill(C d) {
                for (C& c : *this) {
                    static_cast<volatile C&>(c) = d;
                }
            }

            constexpr C& front() noexcept {
                return data()[0];
            }

            constexpr C& back() noexcept {
                return data()[this->size_ - 1];
            }
        };

        template <class D, class C>
        struct basic_storage_vec : public basic_wvec<C>, private helper::omit_empty<D> {
           public:
            constexpr basic_storage_vec() noexcept(noexcept(D{})) = default;

            constexpr basic_storage_vec(basic_wvec<C> c, D&& d) noexcept(noexcept(D{}))
                : basic_wvec<C>(c), helper::omit_empty<D>(std::move(d)) {}

            constexpr explicit basic_storage_vec(basic_wvec<C> c) noexcept(noexcept(D{}))
                : basic_wvec<C>(c) {}

            constexpr basic_storage_vec(basic_storage_vec&& in)
                : basic_wvec<C>(const_cast<C*>(std::exchange(in.data_, nullptr)), std::exchange(in.size_, 0)),
                  helper::omit_empty<D>(std::move(in.om_value())) {}

            constexpr basic_storage_vec& operator=(basic_storage_vec&& in) {
                if (this == &in) {
                    return *this;
                }
                clear();
                this->data_ = std::exchange(in.data_, nullptr);
                this->size_ = std::exchange(in.size_, 0);
                this->move_om_value(std::move(in.om_value()));
                return *this;
            }

            constexpr void clear() {
                if (!this->null()) {
                    this->om_value()(this->data(), this->size_);
                    this->data_ = nullptr;
                    this->size_ = 0;
                }
            }

            constexpr ~basic_storage_vec() {
                clear();
            }
        };

        using rvec = basic_rvec<byte>;
        using wvec = basic_wvec<byte>;
        using rcvec = basic_rvec<char>;
        using wcvec = basic_wvec<char>;
        template <class D>
        using storage_vec = basic_storage_vec<D, byte>;

        template <class C>
        constexpr auto make_copy_fn() {
            return [](basic_wvec<C> dst, basic_rvec<C> src) {
                size_t dst_ = dst.size();
                size_t src_ = src.size();
                size_t min_ = dst_ < src_ ? dst_ : src_;
                for (size_t i = 0; i < min_; i++) {
                    dst[i] = src[i];
                }
                return src_ == dst_ ? 0 : min_ == dst_ ? -1
                                                       : 1;
            };
        }

        // copy copys from src to dst
        // copy returns int
        //  0 if dst.size() == src.size()
        //  1 if dst.size() <  src.size()
        // -1 if dst.size() >  src.size()
        constexpr auto copy = make_copy_fn<byte>();

        template <class C>
        constexpr auto make_shift_fn() {
            return [](basic_wvec<C> range, size_t to, size_t from, size_t len) {
                const auto size = range.size();
                if (to >= size || from > size) {
                    return false;
                }
                if (size - to < len || size - from < len) {
                    return false;
                }
                if (to < from) {
                    for (size_t i = 0; i < len; i++) {
                        range[to + i] = range[from + i];
                    }
                }
                else {
                    for (size_t i = len; i > 0; i--) {
                        range[to + i - 1] = range[from + i - 1];
                    }
                }
                return true;
            };
        }

        constexpr auto shift = make_shift_fn<byte>();

        // for test
        template <class T, size_t len>
        constexpr auto cstr_to_bytes(const T (&val)[len]) {
            static_assert(sizeof(T) == 1);
            struct {
                byte data_[len - 1];

                constexpr size_t size() const {
                    return len - 1;
                }

                constexpr byte* data() {
                    return data_;
                }

                constexpr const byte* data() const {
                    return data_;
                }
            } ret;
            for (size_t i = 0; i < len - 1; i++) {
                ret.data_[i] = val[i];
            }
            return ret;
        }

        constexpr auto default_hash_mask = 0x8f2cf39d98390b;

        template <class C, class F>
        constexpr auto make_hash_fn(F&& f) {
            return [=](view::basic_rvec<C> data, std::uint64_t* s = nullptr, std::uint64_t mask = default_hash_mask) {
                std::uint64_t tmp = 0;
                if (!s) {
                    s = &tmp;
                }
                for (auto b : data) {
                    *s = f(*s, b, mask);
                }
                return *s;
            };
        }

        constexpr auto hash = make_hash_fn<byte>([](std::uint64_t s, byte b, std::uint64_t mask) {
            constexpr auto u64_max = ~std::uint64_t(0);
            auto add = (s << 1);
            if (std::is_constant_evaluated()) {
                if (add > u64_max - b) {
                    byte rem = byte(u64_max - add);
                    add = 0;
                    b -= rem;
                }
            }
            return (add + b) ^ mask;
        });

        namespace test {
            constexpr bool test_shift() {
                byte data[10] = "glspec ol";
                shift(data, 0, 3, 3);
                auto ok = data[0] == 'p' &&
                          data[1] == 'e' &&
                          data[2] == 'c' &&
                          data[3] == 'p';
                if (!ok) {
                    return false;
                }
                data[0] = 'a';
                data[1] = 'n';
                data[2] = 't';
                shift(data, 3, 0, 7);
                ok = data[3] == 'a' &&
                     data[4] == 'n' &&
                     data[5] == 't' &&
                     data[6] == 'p' &&
                     data[7] == 'e' &&
                     data[8] == 'c' &&
                     data[9] == ' ';
                return ok;
            }

            constexpr bool test_zero() {
                byte data[1];
                auto t = view::rvec(data, 0);
                return !t.substr(0, 0).null() && t.substr(1, 0).null();
            }

            static_assert(test_shift());
            static_assert(test_zero());
        }  // namespace test

        template <class C>
        struct basic_swap_wvec {
            basic_wvec<C> w;

            constexpr basic_swap_wvec() noexcept = default;
            constexpr basic_swap_wvec(basic_wvec<C> w) noexcept
                : w(w) {}

            constexpr basic_swap_wvec(basic_swap_wvec&& in) noexcept
                : w(in.w) {
                in.w = {};
            }

            constexpr basic_swap_wvec& operator=(basic_swap_wvec&& in) noexcept {
                if (this == &in) {
                    return *this;
                }
                w = in.w;
                in.w = {};
                return *this;
            }

            constexpr basic_wvec<C> release() noexcept {
                return std::exchange(w, {});
            }

            constexpr ~basic_swap_wvec() noexcept {
                w = {};
            }
        };

        using swap_wvec = basic_swap_wvec<byte>;
        using swap_wcvec = basic_swap_wvec<char>;

    }  // namespace view
}  // namespace futils
