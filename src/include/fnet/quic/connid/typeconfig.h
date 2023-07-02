/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../std/hash_map.h"
#include "../../std/list.h"

namespace utils {
    namespace fnet::quic::connid {
        template <template <class...> class ConnIDMap = slib::hash_map,
                  template <class...> class WaitQue = slib::list>
        struct TypeConfig {
            template <class K, class V>
            using connid_map = ConnIDMap<K, V>;
            template <class V>
            using wait_que = WaitQue<V>;
        };
    }  // namespace fnet::quic::connid
}  // namespace utils