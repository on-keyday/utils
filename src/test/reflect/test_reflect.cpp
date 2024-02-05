/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <reflect/reflect.h>
#include <string>
namespace reflect = futils::reflect;
void test_basic() {
    reflect::test::Test4 test4;
    reflect::test::Test3& test3 = test4;
    reflect::Reflect refl1{test3}, refl2{test4};
    std::uintptr_t val;
    refl1.get(val);
    auto ptr = reinterpret_cast<void**>(val);
    reflect::test::Test3 test32;
    refl2.scan(test32);
    refl2.get(val);
    auto ptr2 = reinterpret_cast<void**>(val);
    refl1.set(ptr2);
    refl1.set(ptr);
    refl2.scan(test4);
    refl2.set(std::uintptr_t(56), offsetof(reflect::test::Test3, val));
    refl2.set(std::uintptr_t(32), offsetof(reflect::test::Test4, val2));
    futils::byte data[10]{1, 2};
    refl2.scan(data);
    auto total_size = refl2.size();
    auto is_array = refl2.traits(reflect::Traits::is_array);
    auto elm_count = refl2.num_elements();
    refl2.underlying(refl1, 1);
    auto elm_size = refl1.size();
    auto num = refl1.get_uint();
    refl2.scan(+data);
    auto ptr_size = refl2.size();
    auto is_ptr = refl2.traits(reflect::Traits::is_pointer);
    refl2.underlying(refl1);
    auto refsize = refl1.size();
}

void test_complex() {
    std::string str = "hello world";
    reflect::Reflect refl{str};
    auto is_str = refl.is<std::string>();
    auto al = refl.align();
    auto nal = refl.num_aligns();
    std::uintptr_t len, cap;
    char* ptr;
    auto static_str = refl.unsafe_ptr_aligned(1);
    str = "reflection interface";
    refl.get_aligned(ptr, 1);
    refl.get_aligned(len, 3);
    refl.get_aligned(cap, 4);
}

void test_vtable_ptr() {
    futils::reflect::test::Test4 t;
    futils::reflect::Reflect r{t};

    auto table = r.get_vtable_ptr();
    table[2] = [](auto a) {

    };
    table[2](r.unsafe_ptr());
}

int main() {
    test_basic();
    test_complex();
    test_vtable_ptr();
}
