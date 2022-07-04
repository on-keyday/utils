/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// core - core quic loops
#pragma once

namespace utils {
    namespace quic {
        namespace core {
            // Looper manages quic loop context
            struct Looper;

            // progress_loop progresses the loop of QUIC connection
            // progress_loop doesn't block operation
            void progress_loop(Looper* l);

        }  // namespace core
    }      // namespace quic
}  // namespace utils
