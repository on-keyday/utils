/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// frame_callback - frame callbacks
#pragma once
#include "types.h"
#include "../mem/callback.h"
#include "../mem/pool.h"

namespace utils {
    namespace quic {
        namespace frame {

            struct ReadContext {
                template <class Ret, class... Args>
                using CB = mem::CB<mem::nocontext_t, Ret, Args...>;
                types prev_type;
                CB<Error, Frame*> frame_cb;

                Error frame(frame::Frame* p) {
                    prev_type = p->type;
                    return frame_cb(p);
                }

                CB<void, Error, const char*, Frame*> frame_error_cb;

                void frame_error(Error err, const char* where, Frame* f) {
                    prev_type = f->type;
                    frame_error_cb(err, where, f);
                }

                CB<void, varint::Error, const char*, Frame*> varint_error_cb;

                void varint_error(varint::Error err, const char* where, Frame* f) {
                    prev_type = f->type;
                    varint_error_cb(err, where, f);
                }

                pool::BytesPool* b;

                allocate::Alloc* get_alloc() {
                    return b ? b->a : nullptr;
                }

                bytes::Bytes get_bytes(tsize s) {
                    if (!b) {
                        return {};
                    }
                    return b->get(s);
                }

                void put_bytes(bytes::Bytes& p) {
                    if (!b) {
                        return;
                    }
                    return b->put(std::move(p));
                }
            };
        }  // namespace frame
    }      // namespace quic
}  // namespace utils
