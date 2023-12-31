/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "interface_list.h"
#include <comb2/tree/simple_node.h>

namespace ifacegen {
    bool traverse(FileData& data, const std::shared_ptr<utils::comb2::tree::node::Node>& node);
}
