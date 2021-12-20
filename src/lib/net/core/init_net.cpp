/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/core/init_net.h"
#include "../../../include/net/core/platform.h"

namespace utils {
    namespace net {

        namespace internal {
            struct InitImpl {
#ifdef _WIN32
                ::WSAData data;
                bool succeed = false;
                InitImpl() {
                    succeed = ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
                }

                ~InitImpl() {
                    ::WSACleanup();
                }
#else
                bool succeed = true;
#endif
            };
            InitImpl& init() {
                static InitImpl net;
                return net;
            }
        }  // namespace internal

        Init& network() {
            static Init init;
            return init;
        }

        Init& placeholder = network();

        Init::Init() {
            internal::init();
        }

        bool initialized() {
            return internal::init().succeed;
        }
    }  // namespace net
}  // namespace utils
