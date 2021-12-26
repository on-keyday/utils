/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// body - http body reading
#pragma once

#include "../../core/sequencer.h"
#include "../core/iodef.h"

namespace utils {
    namespace net {
        namespace h1body {
            enum class BodyType {
                no_info,
                chuncked,
                content_length,
            };

            template <class String, class T>
            bool read_body(String& result, Sequencer<T>& seq) {
            }
        }  // namespace h1body

    }  // namespace net
}  // namespace utils
