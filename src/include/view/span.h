/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>
#include <iterator>
#include <compare>
#include <memory>
#include "../helper/defer.h"

namespace futils {
    namespace view {
        namespace internal {
            template <class T, class U>
            concept is_match_to = requires(T t) {
                { std::data(t) } -> std::convertible_to<U>;
                { std::size(t) } -> std::convertible_to<size_t>;
            };

            template <class T>
            concept has_resize = requires(T t) {
                { t.resize(size_t{}) };
            };
        }  // namespace internal

        template <class T>
        struct rspan {
           protected:
            const T* data_ = nullptr;
            size_t size_ = 0;

           public:
            constexpr rspan() noexcept = default;

            template <class V = T>
            explicit constexpr rspan(const T* p, V&& term = V()) noexcept
                requires(!std::is_same_v<V, size_t>)
                : data_(p) {
                if (!p) {
                    return;
                }
                while (p[size_] != term) {
                    size_++;
                }
            }

            constexpr rspan(const T* p, size_t s) noexcept
                : data_(p), size_(p ? s : 0) {}

            template <class V, std::enable_if_t<internal::is_match_to<V, const T*>, int> = 0>
            constexpr rspan(V&& s) noexcept
                : rspan(std::data(s), std::size(s)) {}

            constexpr const T* cdata() const noexcept {
                return data_;
            }

            constexpr const T* data() const noexcept {
                return cdata();
            }

            constexpr const T* cbegin() const noexcept {
                return data_;
            }

            constexpr const T* cend() const noexcept {
                return data_ + size_;
            }

            constexpr const T* begin() const noexcept {
                return cbegin();
            }

            constexpr const T* end() const noexcept {
                return cend();
            }

            constexpr size_t size() const noexcept {
                return size_;
            }

            constexpr bool empty() const noexcept {
                return size_ == 0;
            }

            constexpr const T& at(size_t i) const noexcept {
                if (i >= size_) {
                    throw "out of range";
                }
                if (!data_) {
                    throw "invalid data";
                }
                return data_[i];
            }

            constexpr const T& front() const noexcept {
                return data_[0];
            }

            constexpr const T& back() const noexcept {
                return data_[size_ - 1];
            }

            constexpr const T& operator[](size_t i) const noexcept {
                return data_[i];
            }

            constexpr bool null() const noexcept {
                return data_ == nullptr;
            }

            constexpr rspan rsubstr(size_t pos = 0, size_t n = ~0) const noexcept {
                if (pos > size_) {
                    return {};  // empty
                }
                if (n > size_ - pos) {
                    n = size_ - pos;
                }
                return rspan(data_ + pos, n);
            }

            constexpr rspan substr(size_t pos = 0, size_t n = ~0) const noexcept {
                return rsubstr(pos, n);
            }

#define ENABLE_EQUAL() \
    template <class Void = void, std::enable_if_t<(std::is_void_v<Void>, std::equality_comparable<T>), int> = 0>

            ENABLE_EQUAL()
            constexpr int compare(const rspan& r) const {
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

            ENABLE_EQUAL()
            constexpr friend auto operator<=>(const rspan& l, const rspan& r) {
                return l.compare(r) <=> 0;
            }

            ENABLE_EQUAL()
            constexpr bool equal(const rspan& r) const {
                if (size_ != r.size_) {
                    return false;
                }
                if (!std::is_constant_evaluated()) {
                    if (data_ == r.data_) {
                        return true;
                    }
                }
                for (size_t i = 0; i < size_; i++) {
                    if (data_[i] != r.data_[i]) {
                        return false;
                    }
                }
                return true;
            }

            ENABLE_EQUAL()
            constexpr friend bool operator==(const rspan& l, const rspan& r) {
                return l.equal(r);
            }
        };
#undef ENABLE_EQUAL
        template <class T>
        struct wspan : rspan<T> {
            constexpr wspan() noexcept = default;
            constexpr wspan(T* d, size_t s) noexcept
                : rspan<T>(d, s) {}

            template <class V, std::enable_if_t<internal::is_match_to<V, T*>, int> = 0>
            constexpr wspan(V&& t) noexcept
                : rspan<T>(t) {}

            constexpr T* data() noexcept {
                return const_cast<T*>(this->data_);
            }

            // because clang can't detect data in basic_rvec
            // redefine this
            constexpr const T* data() const noexcept {
                return this->data_;
            }

            constexpr T& at(size_t i) {
                return const_cast<T&>(rspan<T>::at(i));
            }

            constexpr T& operator[](size_t i) noexcept {
                return data()[i];
            }

            constexpr const T& operator[](size_t i) const noexcept {
                return data()[i];
            }

            constexpr T* begin() noexcept {
                return data();
            }

            constexpr T* end() noexcept {
                return data() + this->size_;
            }

            constexpr wspan wsubstr(size_t pos = 0, size_t n = ~0) const noexcept {
                if (pos > this->size_) {
                    return {};  // empty
                }
                if (n > this->size_ - pos) {
                    n = this->size_ - pos;
                }
                return wspan(const_cast<T*>(this->data_) + pos, n);
            }

            constexpr wspan substr(size_t pos = 0, size_t n = ~0) const noexcept {
                return wsubstr(pos, n);
            }

            constexpr void fill(auto&& d) {
                for (T& c : *this) {
                    c = d;
                }
            }

            constexpr void force_fill(auto&& d) {
                for (T& c : *this) {
                    static_cast<volatile T&>(c) = d;
                }
            }

            constexpr T& front() noexcept {
                return data()[0];
            }

            constexpr T& back() noexcept {
                return data()[this->size_ - 1];
            }
        };

        template <class T>
        struct vspan : wspan<T> {
           private:
            struct base {
                virtual wspan<T> data() = 0;
                virtual bool resize(size_t size) = 0;
                virtual void dealoc() = 0;
                virtual ~base() {}
            };

            template <class O>
            struct derived_new : base {
                O obj;

                constexpr derived_new(auto&& in)
                    : obj(std::forward<decltype(in)>(in)) {}

                constexpr wspan<T> data() override {
                    return wspan<T>(obj);
                }

                constexpr bool resize(size_t size) override {
                    if constexpr (internal::has_resize<O>) {
                        obj.resize(size);
                        return true;
                    }
                    return false;
                }

                constexpr void dealoc() override {
                    delete this;
                }
            };

            template <class O, class Allocator>
            struct derived_allocator : base {
                O obj;
                using R = typename std::allocator_traits<Allocator>::template rebind<derived_allocator>;
                R alloc;

                constexpr derived_allocator(auto&& in, auto&& al)
                    : obj(std::forward<decltype(in)>(in)), alloc(std::forward<decltype(al)>(al)) {}

                constexpr wspan<T> data() override {
                    return wspan<T>(obj);
                }

                constexpr bool resize(size_t size) override {
                    if constexpr (internal::has_resize<O>) {
                        if constexpr (std::convertible_to<decltype(obj.resize()), bool>) {
                            return obj.resize(size);
                        }
                        else {
                            obj.resize(size);
                            return true;
                        }
                    }
                    return false;
                }

                constexpr void dealoc() override {
                    using traits = std::allocator_traits<Allocator>;
                    auto al = std::move(alloc);
                    traits::destroy(al, this);
                    traits::deallocate(al, this);
                }
            };

            base* ctrl = nullptr;

            constexpr void set() {
                auto d = ctrl->data();
                this->data_ = d.data();
                this->size_ = d.size();
            }

           public:
            constexpr vspan() noexcept = default;
            constexpr vspan(T* d, size_t s) noexcept
                : wspan<T>(d, s) {}

            template <class O>
            struct derived_static : base {
                O* ptr;
                constexpr derived_static(O* p)
                    : ptr(p) {}

                constexpr wspan<T> data() override {
                    return wspan<T>(*ptr);
                }

                constexpr bool resize(size_t size) override {
                    if constexpr (internal::has_resize<O>) {
                        ptr->resize(size);
                        return true;
                    }
                    return false;
                }

                constexpr void dealoc() override {}
            };

            template <class V, std::enable_if_t<internal::is_match_to<V, T*>, int> = 0>
            constexpr vspan(derived_static<V>* c) {
                if (!c) {
                    return;
                }
                ctrl = c;
                set();
            }

            template <class V, std::enable_if_t<internal::is_match_to<V, T*>, int> = 0>
            constexpr vspan(V&& t) noexcept {
                ctrl = new derived_new<std::decay_t<V>>(std::forward<V>(t));
                auto d = ctrl->data();
                this->data_ = d.data();
                this->size_ = d.size();
            }

            template <class V, class Alloc, std::enable_if_t<internal::is_match_to<V, T*>, int> = 0>
            constexpr vspan(V&& t, const Alloc& alloc) noexcept {
                using derived = derived_allocator<std::decay_t<V>, Alloc>;
                using bound_alloc = typename std::allocator_traits<Alloc>::template rebind<derived>;
                bound_alloc copy{alloc};
                auto v = copy.allocate(1);
                auto tmp = helper::defer([&] {
                    copy.deallocate(v);
                });
                new (v) derived(std::forward<V>(t), alloc);
                tmp.cancel();
                ctrl = std::launder(v);
                set();
            }

            constexpr bool resize(size_t n) {
                if (!ctrl) {
                    return false;
                }
                if (!ctrl->resize(n)) {
                    return false;
                }
                set();
                return true;
            }

            constexpr bool push_back(auto&& v) {
                if (!resize(this->size_ + 1)) {
                    return false;
                }
                this->data()[this->size_ - 1] = std::forward<decltype(v)>(v);
                return true;
            }

            constexpr vspan& operator=(vspan&& r) {
                if (this == &r) {
                    return *this;
                }
                if (ctrl) {
                    ctrl->dealoc();
                }
                ctrl = std::exchange(r.ctrl, nullptr);
                this->data_ = std::exchange(r.data_, nullptr);
                this->size_ = std::exchange(r.size_, 0);
                return *this;
            }

            constexpr ~vspan() {
                if (ctrl) {
                    ctrl->dealoc();
                }
            }

            constexpr bool has_ctrl() const noexcept {
                return ctrl != nullptr;
            }
        };

        template <class T, class Container>
        struct bgspan : wspan<T> {
           private:
            Container container;

            void set() {
                this->data_ = std::data(container);
                this->size_ = std::size(container);
            }

           public:
            constexpr bgspan() = default;

            template <class Void = void, std::enable_if<(std::is_void_v<Void>, std::is_copy_constructible_v<Container>), int> = 0>
            constexpr bgspan(const Container& r)
                : container(r) {
                set();
            }

            template <class Void = void, std::enable_if<(std::is_void_v<Void>, std::is_move_constructible_v<Container>), int> = 0>
            constexpr bgspan(Container&& r)
                : container(std::move(r)) {
                set();
            }

            template <class Void = void, std::enable_if<(std::is_void_v<Void>, std::is_copy_assignable_v<Container>), int> = 0>
            constexpr bgspan& operator=(const bgspan& r) {
                if (this == &r) {
                    return *this;
                }
                container = r.container;
                set();
            }

            template <class Void = void, std::enable_if<(std::is_void_v<Void>, std::is_move_assignable_v<Container>), int> = 0>
            constexpr bgspan& operator=(bgspan&& r) {
                if (this == &r) {
                    return *this;
                }
                container = std::move(r);
                r.set();
                set();
            }

            constexpr void resize(size_t n) {
                container.resize(n);
            }

            constexpr void push_back(auto&& v) {
                container.push_back(std::forward<decltype(v)>(v));
                set();
            }
        };

        template <class T, template <class...> class Vec>
        using gspan = bgspan<T, Vec<T>>;

        namespace test {
            constexpr bool check_span() {
                wspan<int> span;
                int vec[30]{20, 30};
                int vec2[30]{20, 30, 10};
                span = wspan<int>(vec);
                if (span == vec2) {
                    return false;
                }
                struct L {
                    unsigned char d[2]{0};
                } vec3[10], vec4[10]{};
                auto span2 = wspan<L>(vec3);
                auto span3 = wspan<L>(vec4);
                for (auto& table : span2) {
                }
                return true;
            }

            inline auto check_vspan() {
                vspan<int> gs;
                struct L {
                    int holder[3]{};
                    size_t size_ = 0;

                    constexpr int* data() {
                        return holder;
                    }

                    constexpr bool resize(size_t n) {
                        if (n > 3) {
                            return false;
                        }
                        size_ = n;
                        return true;
                    }

                    constexpr size_t size() const {
                        return size_;
                    }
                } l;
                vspan<int>::derived_static<L> storage{&l};
                gs = vspan<int>(&storage);
                return gs.push_back(32);
            }

            static_assert(check_span());
        }  // namespace test
    }  // namespace view
}  // namespace futils
