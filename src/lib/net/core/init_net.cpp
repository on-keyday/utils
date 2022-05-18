/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/core/init_net.h"
#include "../../../include/net/core/platform.h"
#include "../../../include/net/core/errcode.h"

namespace utils {
    namespace net {

        namespace internal {
            struct InitImpl {
#ifdef _WIN32
                ::WSAData data;
                int succeed;
                InitImpl() {
                    succeed = ::WSAStartup(MAKEWORD(2, 2), &data);
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

        Init::Init() {
            internal::init();
        }

        bool Init::initialized() {
            return internal::init().succeed == 0;
        }

        int Init::errcode() {
            return internal::init().succeed;
        }

        int STDCALL errcode() {
#ifdef _WIN32
            return ::WSAGetLastError();
#else
            return errno;
#endif
        }
    }  // namespace net
}  // namespace utils
