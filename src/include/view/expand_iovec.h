/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// expand_iovec - expanded io vector
#pragma once
#include "iovec.h"
#include <memory>

namespace utils {
    namespace view {
        namespace internal {
            enum class sso_state : byte {
                sta,
                dyn,
            };

            template <size_t size_>
            struct native_array {
                byte data[size_]{};
            };

            template <class Alloc, class C, class U>
            struct basic_sso_storage {
               private:
                union {
                    native_array<sizeof(basic_wvec<C, U>)> sta{};
                    basic_wvec<C, U> dyn;
                };
                sso_state state = sso_state::sta;

                constexpr void move(basic_sso_storage& sso) noexcept {
                    alloc = std::move(sso.alloc);
                    if (sso.state == sso_state::dyn) {
                        dyn = sso.dyn;
                    }
                    else {
                        sta = sso.sta;
                    }
                    state = sso.state;
                    sso.sta = {};
                    sso.state = sso_state::sta;
                }

               public:
                Alloc alloc{};

                constexpr basic_sso_storage() {}

                constexpr basic_sso_storage(basic_sso_storage&& sso) {
                    move(sso);
                }

                constexpr basic_sso_storage& operator=(basic_sso_storage&& sso) {
                    move(sso);
                    return *this;
                }

                constexpr bool is_dyn() const {
                    return state == sso_state::dyn;
                }

                constexpr void set_dyn(C* dat, size_t size) noexcept {
                    dyn = basic_wvec<C, U>{dat, size};
                    state = sso_state::dyn;
                }

                constexpr bool set_sta(const C* dat, size_t size) noexcept {
                    if (sizeof(sta) < size) {
                        return false;
                    }
                    for (auto i = 0; i < size; i++) {
                        sta.data[i] = dat[i];
                    }
                    state = sso_state::sta;
                    return true;
                }

                constexpr basic_wvec<C, U> wbuf() noexcept {
                    if (state == sso_state::dyn) {
                        return dyn;
                    }
                    else {
                        return basic_wvec<C, U>(sta.data);
                    }
                }

                constexpr basic_rvec<C, U> rbuf() const noexcept {
                    if (state == sso_state::dyn) {
                        return dyn;
                    }
                    else {
                        return basic_rvec<C, U>(sta.data);
                    }
                }

                constexpr ~basic_sso_storage() {}
            };

        }  // namespace internal

        template <class Alloc, class C, class U>
        struct basic_expand_storage_vec {
           private:
            internal::basic_sso_storage<Alloc, C, U> data_;
            size_t size_ = 0;
            using traits = std::allocator_traits<Alloc>;

            constexpr void copy_from_rvec(basic_rvec<C, U> input, const Alloc& inalloc) {
                size_ = input.size();
                data_.alloc = inalloc;
                if (data_.set_sta(input.data(), size_)) {
                    return;
                }
                // ptr must not be nullptr
                C* ptr = traits::allocate(data_.alloc, size_);
                auto p = ptr;
                for (auto d : input) {
                    *p = d;
                    p++;
                }
                data_.set_dyn(ptr, size_);
            }

            constexpr void copy_in_place(basic_rvec<C, U> src) {
                resize_nofill(src.size());
                auto wb = data_.wbuf();
                for (auto i = 0; i < src.size(); i++) {
                    wb[i] = src[i];
                }
            }

            constexpr void free_dyn() {
                if (data_.is_dyn()) {
                    auto wbuf = data_.wbuf();
                    traits::deallocate(data_.alloc, wbuf.data(), wbuf.size());
                }
            }

           public:
            constexpr basic_expand_storage_vec() = default;

            constexpr basic_expand_storage_vec(basic_rvec<C, U> r) {
                copy_in_place(r);
            }

            constexpr basic_expand_storage_vec(Alloc&& al, basic_rvec<C, U> r) {
                data_.alloc = std::move(al);
                copy_in_place(r);
            }

            constexpr basic_expand_storage_vec(basic_expand_storage_vec&& in)
                : data_(std::exchange(in.data_, internal::basic_sso_storage<Alloc, C, U>{})),
                  size_(std::exchange(in.size_, 0)) {}

            constexpr basic_expand_storage_vec(const basic_expand_storage_vec& in) {
                copy_from_rvec(in.data_.rbuf().substr(0, in.size_), in.data_.alloc);
            }

            constexpr basic_expand_storage_vec& operator=(const basic_expand_storage_vec& in) {
                if (this == &in) {
                    return *this;
                }
                if (data_.alloc != in.data_.alloc) {
                    free_dyn();
                    copy_from_rvec(in.data_.rbuf().substr(0, in.size_), in.data_.alloc);
                    return *this;
                }
                copy_in_place(in.data_.rbuf().substr(0, in.size_));
                return *this;
            }

            constexpr basic_expand_storage_vec& operator=(basic_expand_storage_vec&& in) {
                if (this == &in) {
                    return *this;
                }
                data_ = std::exchange(in.data_, internal::basic_sso_storage<Alloc, C, U>{});
                size_ = std::exchange(in.size_, 0);
                return *this;
            }

            constexpr ~basic_expand_storage_vec() {
                free_dyn();
            }

            constexpr size_t capacity() const {
                return data_.rbuf().size();
            }

            constexpr void reserve(size_t new_cap) {
                auto buf = data_.wbuf();
                if (new_cap <= buf.size()) {
                    return;  // already exists
                }
                C* new_ptr = traits::allocate(data_.alloc, new_cap);
                for (size_t i = 0; i < size_; i++) {
                    new_ptr[i] = buf[i];
                }
                free_dyn();
                data_.set_dyn(new_ptr, new_cap);
            }

            constexpr void shrink_to_fit() {
                if (!data_.is_dyn()) {
                    return;
                }
                auto buf = data_.wbuf();
                if (buf.size() == size_) {
                    return;
                }
                if (!data_.set_sta(buf.data(), size_)) {
                    C* new_ptr = traits::allocate(data_.alloc, size_);
                    for (auto i = 0; i < size_; i++) {
                        new_ptr[i] = buf[i];
                    }
                    data_.set_dyn(new_ptr, size_);
                }
                traits::deallocate(data_.alloc, buf.data(), buf.size());
            }

           private:
            constexpr size_t resize_nofill(size_t new_size) {
                auto old_size = size_;
                if (new_size <= size_) {
                    size_ = new_size;
                    return old_size;  // already exists
                }
                if (capacity() < new_size) {
                    reserve(size_ * 2 > new_size ? size_ * 2 : new_size);
                }
                size_ = new_size;
                return old_size;
            }

           public:
            constexpr void resize(size_t new_size) {
                auto old_size = resize_nofill(new_size);
                auto buf = data_.wbuf();
                for (auto i = old_size; i < new_size; i++) {
                    buf[i] = 0;
                }
            }

            constexpr size_t size() const noexcept {
                return size_;
            }

            constexpr const C* data() const noexcept {
                return data_.rbuf().data();
            }

            constexpr const C* cdata() const noexcept {
                return data_.rbuf().cdata();
            }

            constexpr C* data() noexcept {
                return data_.wbuf().data();
            }

            const U* as_char() const noexcept {
                return data_.rbuf().as_char();
            }

            U* as_char() noexcept {
                return data_.wbuf().as_char();
            }

            constexpr const C* begin() const noexcept {
                return data_.rbuf().begin();
            }

            constexpr const C* end() const noexcept {
                return begin() + size_;
            }

            constexpr const C* cbegin() const noexcept {
                return data_.rbuf().cbegin();
            }

            constexpr const C* cend() const noexcept {
                return cbegin() + size_;
            }

            constexpr C* begin() noexcept {
                return data_.wbuf().begin();
            }

            constexpr C* end() noexcept {
                return begin() + size_;
            }

            constexpr C& operator[](size_t i) {
                return data_.wbuf()[i];
            }

            constexpr const C& operator[](size_t i) const {
                return data_.rbuf()[i];
            }

            constexpr basic_wvec<C, U> wvec() {
                return data_.wbuf().substr(0, size_);
            }

            constexpr basic_rvec<C, U> rvec() const {
                return data_.rbuf().substr(0, size_);
            }

            constexpr void push_back(C c) {
                resize_nofill(size_ + 1);
                *(end() - 1) = c;
            }

            constexpr basic_expand_storage_vec& append(auto&& in) {
                auto old_size = resize_nofill(size_ + std::size(in));
                auto wbuf = data_.wbuf();
                for (size_t i = old_size; i < size_; i++) {
                    wbuf[i] = in[i - old_size];
                }
                return *this;
            }

            constexpr basic_expand_storage_vec& append(const U* ptr) {
                return append(view::rvec(ptr));
            }

            constexpr basic_expand_storage_vec& append(const C* ptr, size_t len) {
                return append(view::rvec(ptr, len));
            }

            constexpr int compare(const basic_expand_storage_vec& in) {
                return rvec().compare(in.rvec());
            }

            constexpr friend bool operator==(const basic_expand_storage_vec& a, const basic_expand_storage_vec& b) {
                return a.rvec() == b.rvec();
            }

            constexpr friend auto operator<=>(const basic_expand_storage_vec& a, const basic_expand_storage_vec& b) {
                return a.rvec() <=> b.rvec();
            }

            constexpr void fill(C c) {
                wvec().fill(c);
            }

            constexpr void force_fill(C c) {
                wvec().force_fill(c);
            }

            constexpr bool shift_front(size_t n) {
                constexpr auto shift_fn = view::make_shift_fn<C, U>();
                auto range = wvec();
                if (!shift_fn(range, 0, n, range.size() - n)) {
                    return false;
                }
                resize(range.size() - n);
                return true;
            }

            // call resize(size()+add) then return old size()
            constexpr size_t expand(size_t add) {
                const auto old = size_;
                resize(size_ + add);
                return old;
            }

            constexpr const C& front() const noexcept {
                return wvec().front();
            }

            constexpr const C& back() const noexcept {
                return wvec().back();
            }

            constexpr C& front() noexcept {
                return wvec().front();
            }

            constexpr C& back() noexcept {
                return wvec().back();
            }
        };

        template <class Alloc>
        using expand_storage_vec = basic_expand_storage_vec<Alloc, byte, char>;

    }  // namespace view
}  // namespace utils
