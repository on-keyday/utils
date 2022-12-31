/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// iovec - io vector
#pragma once
#include "../core/strlen.h"
#include "../core/byte.h"
#include <cstddef>
#include <concepts>
#include <iterator>
#include <compare>

namespace utils {
    namespace view {

        namespace internal {

            template <class T>
            concept non_const_ptr = std::is_pointer_v<T> &&
                                    std::is_const_v<std::remove_pointer_t<T>> == false;

            template <class T>
            concept is_not_const_data = requires(T t) {
                                            { std::data(t) } -> non_const_ptr;
                                        };
        }  // namespace internal

        template <class C, class U>
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
                : data_(d), size_(d ? utils::strlen(d) : 0) {}

            basic_rvec(const U* c, size_t s) noexcept
                : basic_rvec(reinterpret_cast<const C*>(c), s) {}

            basic_rvec(const U* c)
                : basic_rvec(reinterpret_cast<const C*>(c)) {}

            template <class T>
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
                if (pos >= size_) {
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
                    return size_ == r.size_ ? 0 : size_ < r.size_ ? 1
                                                                  : -1;
                }
                return data_[i] < r.data_[i] ? 1 : -1;
            }

            constexpr friend std::strong_ordering operator<=>(const basic_rvec& l, const basic_rvec& r) {
                return l.compare(r) <=> 0;
            }
        };

        template <class C, class U>
        struct basic_wvec : public basic_rvec<C, U> {
            constexpr basic_wvec() noexcept = default;
            constexpr basic_wvec(C* d, size_t s) noexcept
                : basic_rvec<C, U>(d, s) {}
            basic_wvec(U* d, size_t s) noexcept
                : basic_rvec<C, U>(d, s) {}

            template <internal::is_not_const_data T>
            constexpr basic_wvec(T&& t) noexcept
                : basic_rvec<C, U>(t) {}

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

            constexpr C* begin() noexcept {
                return data();
            }

            constexpr C* end() noexcept {
                return data() + this->size_;
            }

            U* as_char() noexcept {
                return reinterpret_cast<U*>(data());
            }

            constexpr basic_wvec wsubstr(size_t pos = 0, size_t n = ~0) const noexcept {
                if (pos >= this->size_) {
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
        };

        template <class D, class C, class U>
        struct basic_storage_vec : public basic_wvec<C, U> {
           private:
            D del{};

           public:
            constexpr basic_storage_vec() noexcept(noexcept(D{})) = default;

            constexpr basic_storage_vec(C* c, size_t s, D&& d) noexcept(noexcept(D{}))
                : del(std::move(d)), basic_wvec<C, U>(c, s) {}

            basic_storage_vec(U* c, size_t s, D&& d) noexcept(noexcept(D{}))
                : del(std::move(d)), basic_wvec<C, U>(c, s) {}

            constexpr basic_storage_vec(C* c, size_t s) noexcept(noexcept(D{}))
                : basic_wvec<C, U>(c, s) {}

            basic_storage_vec(U* c, size_t s) noexcept(noexcept(D{}))
                : basic_wvec<C, U>(c, s) {}

            constexpr basic_storage_vec(basic_storage_vec&& in)
                : del(std::exchange(in.del, D{})),
                  basic_wvec<C, U>(const_cast<byte*>(std::exchange(in.data_, nullptr)), std::exchange(in.size_, 0)) {}

            basic_storage_vec& operator=(basic_storage_vec&& in) {
                if (this == &in) {
                    return *this;
                }
                this->~basic_storage_vec();
                this->data_ = std::exchange(in.data_, nullptr);
                this->size_ = std::exchange(in.size_, 0);
                this->del = std::exchange(in.del, {});
                return *this;
            }

            ~basic_storage_vec() {
                if (!this->null()) {
                    del(this->data(), this->size_);
                }
            }
        };

        using rvec = basic_rvec<byte, char>;
        using wvec = basic_wvec<byte, char>;
        template <class D>
        using storage_vec = basic_storage_vec<D, byte, char>;

        template <class C, class U>
        constexpr auto make_copy_fn() {
            return [](basic_wvec<C, U> dst, basic_rvec<C, U> src) {
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
        constexpr auto copy = make_copy_fn<byte, char>();

    }  // namespace view
}  // namespace utils
