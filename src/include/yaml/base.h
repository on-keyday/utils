/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// base - yaml base definition
#pragma once
#include "../core/byte.h"

#include <vector>
#include <optional>
// see also https://yaml.org/spec/1.2.2/
namespace utils::yaml {

    enum class NodeKind : byte {
        doc = 0x1,
        seq = 0x2,
        map = 0x4,
        scalar = 0x8,
        alias = 0x10,
    };

    enum class Style : byte {
        tagged = 0x1,
        double_quoted = 0x2,
        single_quoted = 0x4,
        literal = 0x8,
        folded = 0x10,
        flow = 0x20,
    };

    struct Unspec {};

    template <class S, template <class...> class V>
    struct Node {
        using pNode = std::shared_ptr<Node>;
        S value;
        std::optional<V<pNode>> contents;
    };

}  // namespace utils::yaml
