/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// c_context - c language context
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
// DnetCContext is context values that holds all
// resource from dnet
struct DnetCContext;

DnetCContext* dnet_create();
void dnet_delete(DnetCContext* c);

#ifdef __cplusplus
}
#endif
