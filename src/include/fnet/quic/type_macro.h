/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#define QUIC_ctx_vector_type(name, TConfig, ref) \
    template <class T>                           \
    using name = typename TConfig::template ref<T>
#define QUIC_ctx_map_type(name, TConfig, ref) \
    template <class K, class V>               \
    using name = typename TConfig::template ref<K, V>
#define QUIC_ctx_type(name, TConfig, ref) \
    using name = typename TConfig::ref