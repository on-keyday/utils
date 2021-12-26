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
#include "../../helper/readutil.h"

namespace utils {
    namespace net {
        namespace h1body {
            enum class BodyType {
                no_info,
                chuncked,
                content_length,
            };

            template <class String, class T>
            bool read_body(String& result, Sequencer<T>& seq, BodyType type = BodyType::no_info, size_t expect = 0) {
                auto inipos = seq.rptr;
                if (type == BodyType::no_info) {
                    helper::read_all(result, seq);
                    return true;
                }
            }
        }  // namespace h1body

    }  // namespace net
}  // namespace utils
