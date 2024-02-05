/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace futils::thread::poll {
    enum class PollState {
        pending,
        ready,
    };

    struct Poller {
       private:
        void* data = nullptr;
        void (*poll)(void*);

       public:
    };

    struct Event {
    };

}  // namespace futils::thread::poll