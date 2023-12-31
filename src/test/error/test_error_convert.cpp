/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <error/error.h>
#include <memory>

template <class T>
struct my_allocator : std::allocator<T> {
    template <class U>
    struct rebind {
        typedef my_allocator<U> other;
    };

    template <class U>
    my_allocator(const my_allocator<U>&) {}

    my_allocator() = default;

    template <class U>
    my_allocator(const std::allocator<U>&) {}

    template <class U>
    my_allocator(std::allocator<U>&&) {}

    template <class U>
    my_allocator& operator=(const std::allocator<U>&) {
        return *this;
    }

    template <class U>
    my_allocator& operator=(std::allocator<U>&&) {
        return *this;
    }
};

struct CustomError {
    int a;

    constexpr bool operator==(const CustomError& other) const {
        return a == other.a;
    }

    void error(auto&& b) {
        utils::strutil::append(b, "error");
    }
};

struct UnwrapError {
    int a;

    constexpr bool operator==(const UnwrapError& other) const {
        return a == other.a;
    }

    void error(auto&& b) {
        utils::strutil::append(b, "error");
    }

    CustomError unwrap() {
        return CustomError{.a = a};
    }
};

struct Unwrap2 {
    void error(auto&& b) {
        utils::strutil::append(b, "error");
    }
    utils::error::Error<std::allocator<utils::byte>> unwrap() {
        return utils::error::Error<std::allocator<utils::byte>>("test", utils::error::Category::none);
    }
};

int main() {
    namespace error = utils::error;
    error::Error<std::allocator<utils::byte>> e;
    error::Error<my_allocator<utils::byte>> e2;
    // non allocation conversion
    e = error::Error<std::allocator<utils::byte>>("test", error::Category::none);
    e2 = e;
    // different allocator Error can compare
    e = CustomError{.a = 1};
    e2 = CustomError{.a = 1};
    assert(e == e2);
    // different allocator Error can copy
    e = UnwrapError{.a = 1};
    e2 = e;
    // and compare and it is same
    assert(e == e2);
    // and unwrap
    auto e3 = e2.unwrap();
    assert(e3);
    e2 = CustomError{.a = 1};
    assert(e2 == e3);
    e = CustomError{.a = 1};
    assert(e == e3);
    auto p = e3.as<CustomError>();
    assert(p);
    assert(p->a == 1);
    p->a = 2;
    assert(e2 != e3);

    e = Unwrap2{};
    e2 = std::move(e);
    e3 = e2.unwrap();
}
