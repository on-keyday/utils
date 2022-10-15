/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <deprecated/quic/mem/hash_map.h>
#include <deprecated/quic/conn/connId.h>
#include <deprecated/quic/internal/context_internal.h>
#include <string>

void test_quic_hash_map() {
    using namespace utils::quic;
    mem::HashMap<std::string, std::string, mem::EmptyLock> c;
    auto a = allocate::stdalloc();
    c.set_alloc(&a);
    c.rehash_nlock(10);
    c.insert_nlock("key1", "hello");
    c.insert_nlock("key2", "help");
    c.remove_nlock("key1");
    c.insert_nlock("key3", "how old are you?");
    c.rehash_nlock(20);
    c.replace_nlock("key2", "document");

    c.for_each_nlock([](auto& key, auto& value) {
        return true;
    });

    auto kv = c.get("key1");
}

int main() {
    test_quic_hash_map();
}