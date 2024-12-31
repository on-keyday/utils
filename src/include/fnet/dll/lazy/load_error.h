/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <fnet/error.h>
#include "lazy.h"

namespace futils::fnet::lazy {

    error::Error load_error(const char* msg);
}