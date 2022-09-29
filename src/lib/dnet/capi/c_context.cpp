/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <dnet/dll/glheap.h>
#include <memory>
#include <dnet/dll/capi.h>
#include <dnet/capi/c_context.h>

using namespace utils::dnet;
#ifdef __cplusplus
extern "C" {
#endif

struct RsList {
    capi::typed_resource<RsList> next;
    capi::generic_resource resource;
};

struct DnetCContext {
    capi::typed_resource<RsList> root;
    capi::typed_resource<RsList> current;
};

DnetCContext* dnet_create() {
    return new_from_global_heap<DnetCContext>();
}

void dnet_delete(DnetCContext* c) {
    delete_with_global_heap(c);
}

#ifdef __cplusplus
}
#endif

namespace utils {
    namespace dnet {
        namespace capi {
            bool add_resource(DnetCContext* c, generic_resource res) {
                if (!c) {
                    return false;
                }
                auto list = make_resource<RsList>();
                if (!list) {
                    return false;
                }
                list->resource = std::move(res);
                if (!c->root) {
                    c->root = list;
                }
                else {
                    c->current->next = list;
                }
                c->current = list;
                return true;
            }
        }  // namespace capi
    }      // namespace dnet
}  // namespace utils
