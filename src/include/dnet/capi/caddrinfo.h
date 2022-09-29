/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// caddrinfo - c language inteface to addrinfo
#pragma once
#include "c_context.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DCWaitAddrInfo;

struct DCSockAddr {
    union {
        struct {
            const void* addr;
            int addrlen;
        };
        struct {
            const char* hostname;
            int namelen;
        };
    };
    int af;
    int type;
    int proto;
    int flag;
};

DCWaitAddrInfo* dnet_resolve_address(DnetCContext* c, DCSockAddr addr, const char* port);

#ifdef __cplusplus
}
#endif
