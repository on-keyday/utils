/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// expand_iovec - expanded io vector
#pragma once
#include "../core/byte.h"
#include "../binary/flags.h"
#include "iovec.h"
#include <memory>
#include <new>

namespace utils {
    namespace view {
        namespace internal {
            enum class sso_state : byte {
                sta = 0,
                dyn,
            };

            template <class C, size_t size_>
            struct native_array {
                C data[size_]{};
            };

            template <class Alloc, class C>
            struct basic_sso_storage : helper::omit_empty<Alloc> {
               private:
                union {
                    native_array<C, sizeof(basic_wvec<C>)> sta{};
                    basic_wvec<C> dyn;
                };
                binary::flags_t<size_t, 1, sizeof(size_t) * bit_per_byte - 1> state_;

                bits_flag_alias_method_with_enum(state_, 0, state, sso_state);

                constexpr void move(basic_sso_storage& sso) noexcept {
                    this->move_om_value(std::move(sso.om_value()));
                    if (sso.state() == sso_state::dyn) {
                        dyn = sso.dyn;
                    }
                    else {
                        sta = sso.sta;
                    }
                    state_ = sso.state_;
                    sso.sta = {};
                    sso.state_ = 0;
                }

               public:
                constexpr basic_sso_storage(Alloc&& alloc) noexcept
                    : helper::omit_empty<Alloc>{std::move(alloc)} {}

                constexpr basic_sso_storage(const Alloc& alloc)
                    : helper::omit_empty<Alloc>{alloc} {}

                constexpr basic_sso_storage() {}

                bits_flag_alias_method(state_, 1, size);

                constexpr basic_sso_storage(basic_sso_storage&& sso) {
                    move(sso);
                }

                constexpr basic_sso_storage& operator=(basic_sso_storage&& sso) {
                    move(sso);
                    return *this;
                }

                constexpr bool is_dyn() const {
                    return state() == sso_state::dyn;
                }

                constexpr void set_dyn(C* dat, size_t size) noexcept {
                    dyn = basic_wvec<C>{dat, size};
                    state(sso_state::dyn);
                }

                static constexpr size_t sso_size() noexcept {
                    return sizeof(sta);
                }

                constexpr bool set_sta(const C* dat, size_t size) noexcept {
                    if (sizeof(sta) < size) {
                        return false;
                    }
                    for (auto i = 0; i < size; i++) {
                        sta.data[i] = dat[i];
                    }
                    state(sso_state::sta);
                    return true;
                }

                constexpr basic_wvec<C> wbuf() noexcept {
                    if (state() == sso_state::dyn) {
                        return dyn;
                    }
                    else {
                        return basic_wvec<C>(sta.data, sizeof(sta.data));
                    }
                }

                constexpr basic_rvec<C> rbuf() const noexcept {
                    if (state() == sso_state::dyn) {
                        return dyn;
                    }
                    else {
                        return basic_rvec<C>(sta.data, sizeof(sta.data));
                    }
                }

                constexpr ~basic_sso_storage() {}
            };

            template <class T>
            concept has_too_large_error = requires(T t) {
                t.too_large_error();
            };

        }  // namespace internal

        template <class Alloc, class C>
        struct basic_expand_storage_vec {
           private:
            using storage = internal::basic_sso_storage<Alloc, C>;
            storage data_;
            using traits = std::allocator_traits<Alloc>;

            [[noreturn]] void handle_too_large() {
                if constexpr (internal::has_too_large_error<Alloc>) {
                    data_.om_value().too_large_error();
                }
                auto handler = std::get_new_handler();
                if (!handler) {
                    throw std::bad_alloc();
                }
                handler();
                throw std::bad_alloc();
                __builtin_unreachable();
            }

            C* alloc_(size_t size) {
                decltype(data_.om_value()) alloc = data_.om_value();
                // ptr must not be nullptr
                return traits::allocate(alloc, size);
            }

            void dealloc_(C* c, size_t size) {
                decltype(data_.om_value()) alloc = data_.om_value();
                traits::deallocate(alloc, c, size);
            }

            constexpr void copy_from_rvec(basic_rvec<C> input) {
                while (!data_.size(input.size())) {
                    handle_too_large();
                }
                if (data_.set_sta(input.data(), input.size())) {
                    return;
                }
                // ptr must not be nullptr
                C* ptr = alloc_(input.size());
                auto p = ptr;
                for (auto d : input) {
                    *p = d;
                    p++;
                }
                data_.set_dyn(ptr, input.size());
            }

            constexpr void copy_in_place(basic_rvec<C> src) {
                resize_nofill(src.size());
                auto wb = data_.wbuf();
                for (auto i = 0; i < src.size(); i++) {
                    wb[i] = src[i];
                }
            }

            constexpr void free_dyn() {
                if (data_.is_dyn()) {
                    auto wbuf = data_.wbuf();
                    dealloc_(wbuf.data(), wbuf.size());
                }
            }

           public:
            constexpr basic_expand_storage_vec() = default;

            constexpr basic_expand_storage_vec(basic_rvec<C> r) {
                copy_in_place(r);
            }

            constexpr basic_expand_storage_vec(Alloc&& al, basic_rvec<C> r)
                : data_(std::move(al)) {
                copy_in_place(r);
            }

            constexpr basic_expand_storage_vec(basic_expand_storage_vec&& in)
                : data_(std::exchange(in.data_, storage{})) {}

            constexpr basic_expand_storage_vec(const basic_expand_storage_vec& in)
                : data_(in.data_.om_value()) {
                copy_from_rvec(in.rvec());
            }

            constexpr basic_expand_storage_vec& operator=(const basic_expand_storage_vec& in) {
                if (this == &in) {
                    return *this;
                }
                if (data_.om_value() != in.data_.om_value()) {
                    free_dyn();
                    data_.copy_om_value(in.data_.om_value());
                    copy_from_rvec(in.rvec());
                    return *this;
                }
                copy_in_place(in.rvec());
                return *this;
            }

            constexpr basic_expand_storage_vec& operator=(basic_expand_storage_vec&& in) {
                if (this == &in) {
                    return *this;
                }
                data_ = std::exchange(in.data_, storage{});
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
                C* new_ptr = alloc_(new_cap);
                for (size_t i = 0; i < data_.size(); i++) {
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
                if (buf.size() == data_.size()) {
                    return;
                }
                if (!data_.set_sta(buf.data(), data_.size())) {
                    C* new_ptr = alloc_(data_.size());
                    for (auto i = 0; i < data_.size(); i++) {
                        new_ptr[i] = buf[i];
                    }
                    data_.set_dyn(new_ptr, data_.size());
                }
                dealloc_(buf.data(), buf.size());
            }

            static constexpr size_t sso_size() noexcept {
                return storage::sso_size();
            }

           private:
            constexpr size_t resize_nofill(size_t new_size) {
                if (new_size > data_.size_max) {
                    handle_too_large();
                }
                auto old_size = data_.size();
                if (new_size <= old_size) {
                    data_.size(new_size);  // must return true
                    return old_size;       // already exists
                }
                if (capacity() < new_size) {
                    reserve(data_.size() * 2 > new_size ? data_.size() * 2 : new_size);
                }
                data_.size(new_size);  // must return true
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
                return data_.size();
            }

            static constexpr size_t max_size() noexcept {
                return storage::size_max;
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

            template <class U = internal::default_as_char<C>>
            const U* as_char() const noexcept {
                return data_.rbuf().as_char();
            }

            template <class U = internal::default_as_char<C>>
            U* as_char() noexcept {
                return data_.wbuf().as_char();
            }

            constexpr const C* begin() const noexcept {
                return data_.rbuf().begin();
            }

            constexpr const C* end() const noexcept {
                return begin() + size();
            }

            constexpr const C* cbegin() const noexcept {
                return data_.rbuf().cbegin();
            }

            constexpr const C* cend() const noexcept {
                return cbegin() + size();
            }

            constexpr C* begin() noexcept {
                return data_.wbuf().begin();
            }

            constexpr C* end() noexcept {
                return begin() + size();
            }

            constexpr C& operator[](size_t i) {
                return data_.wbuf()[i];
            }

            constexpr const C& operator[](size_t i) const {
                return data_.rbuf()[i];
            }

            constexpr basic_wvec<C> wvec() {
                return data_.wbuf().substr(0, size());
            }

            constexpr basic_rvec<C> rvec() const {
                return data_.rbuf().substr(0, size());
            }

            constexpr void push_back(C c) {
                resize_nofill(size() + 1);
                *(end() - 1) = c;
            }

            constexpr basic_expand_storage_vec& append(auto&& in) {
                auto old_size = resize_nofill(size() + std::size(in));
                auto wbuf = data_.wbuf();
                for (size_t i = old_size; i < size(); i++) {
                    wbuf[i] = in[i - old_size];
                }
                return *this;
            }

            template <class U = internal::default_as_char<C>>
            constexpr basic_expand_storage_vec& append(const U* ptr) {
                struct {
                    const U* p = nullptr;
                    size_t size_ = 0;
                    constexpr U operator[](size_t i) const {
                        return p[i];
                    }
                    constexpr size_t size() const {
                        return size_;
                    }
                } l{ptr, utils::strlen(ptr)};
                return append(l);
            }

            constexpr basic_expand_storage_vec& append(const C* ptr) {
                return append(basic_rvec<C>(ptr));
            }

            constexpr basic_expand_storage_vec& append(const C* ptr, size_t len) {
                return append(basic_rvec<C>(ptr, len));
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

            constexpr friend bool operator==(const basic_expand_storage_vec& a, const basic_rvec<C>& b) {
                return a.rvec() == b;
            }

            constexpr friend auto operator<=>(const basic_expand_storage_vec& a, const basic_rvec<C>& b) {
                return a.rvec() <=> b;
            }

            constexpr void fill(C c) {
                wvec().fill(c);
            }

            constexpr void force_fill(C c) {
                wvec().force_fill(c);
            }

            constexpr bool shift_front(size_t n) {
                constexpr auto shift_fn = view::make_shift_fn<C>();
                auto range = wvec();
                if (!shift_fn(range, 0, n, range.size() - n)) {
                    return false;
                }
                resize(range.size() - n);
                return true;
            }

            // call resize(size()+add) then return old size()
            constexpr size_t expand(size_t add) {
                const auto old = size();
                resize(size() + add);
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

            constexpr bool empty() const noexcept {
                return size() == 0;
            }
        };

        template <class Alloc>
        using expand_storage_vec = basic_expand_storage_vec<Alloc, byte>;

    }  // namespace view
}  // namespace utils
