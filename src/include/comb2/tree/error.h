/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <string>
#include "../pos.h"
#include <vector>
namespace futils::comb2::tree::node {
    struct Error {
        std::string err;
        Pos pos;
    };

    struct Errors {
        std::vector<Error> errs;

        void push(Error&& err) {
            errs.push_back(std::move(err));
        }
    };
}  // namespace futils::comb2::tree::node