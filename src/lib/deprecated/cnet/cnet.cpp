/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <deprecated/cnet/cnet.h>

namespace utils {
    namespace cnet {
        enum class InternalFlag {
            none,
            initialized = 0x1,
            init_succeed = 0x2,
            uninitialized = 0x4,
        };

        DEFINE_ENUM_FLAGOP(InternalFlag)

        struct count_stopper {
            size_t count;

            bool stop() {
                if (count == 0) {
                    return true;
                }
                count--;
                return count == 0;
            }
        };

        struct CNet {
            void* user;
            CNet* next;
            CNet* parent;
            Protocol proto;
            CNetFlag flag;
            InternalFlag inflag;

            ~CNet() {
                count_stopper stop{200};
                uninitialize(stop, this);
                proto.deleter(user);
                delete next;
            }
        };

        bool STDCALL reset_protocol_context(CNet* ctx, CNetFlag flag, void* user, const Protocol* proto) {
            if (!ctx || !user || !proto || !proto->deleter) {
                return false;
            }
            ctx->flag = flag;
            ctx->user = user;
            ctx->proto = *proto;
            delete ctx->next;
            ctx->next = nullptr;
            if (ctx->proto.initialize) {
                if (any(flag & CNetFlag::init_before_io)) {
                    ctx->inflag = InternalFlag::none;
                }
                else {
                    if (auto err = ctx->proto.initialize({}, ctx, user); err != nullptr) {
                        ctx->proto.deleter(user);
                        ctx->user = nullptr;
                        ctx->proto = {};
                        ctx->proto.deleter = [](void*) {};
                        return false;
                    }
                    ctx->inflag = InternalFlag::initialized | InternalFlag::init_succeed;
                }
            }
            else {
                ctx->inflag = InternalFlag::init_succeed;
            }
            return true;
        }

        CNet* STDCALL create_cnet(CNetFlag flag, void* user, const Protocol* proto) {
            if (!user || !proto || !proto->deleter) {
                return nullptr;
            }
            auto ctx = new CNet{};
            if (!reset_protocol_context(ctx, flag, user, proto)) {
                delete ctx;
                return nullptr;
            }
            ctx->parent = nullptr;
            return ctx;
        }

        bool STDCALL set_lowlevel_protocol(CNet* ctx, CNet* protocol) {
            if (!ctx || ctx == protocol) {
                return false;
            }
            if (any(ctx->flag & CNetFlag::final_link)) {
                return false;
            }
            if (ctx->next && any(ctx->flag & CNetFlag::once_set_no_delete_link)) {
                return false;
            }
            delete ctx->next;
            ctx->next = protocol;
            if (protocol) {
                protocol->parent = ctx;
            }
            return true;
        }

        CNet* STDCALL get_lowlevel_protocol(CNet* ctx) {
            if (!ctx) {
                return nullptr;
            }
            return ctx->next;
        }

        Error STDCALL initialize(Stopper stop, CNet* ctx) {
            if (!ctx) {
                return consterror{"ctx is nullptr"};
            }
            if (any(ctx->flag & CNetFlag::reinitializable)) {
                if (any(ctx->inflag & InternalFlag::uninitialized)) {
                    ctx->inflag = InternalFlag::none;
                }
            }
            Error err;
            if (ctx->proto.initialize && !any(ctx->inflag & InternalFlag::initialized)) {
                if (auto err = ctx->proto.initialize(stop, ctx, ctx->user); err == nullptr) {
                    ctx->inflag |= InternalFlag::init_succeed;
                }
                ctx->inflag |= InternalFlag::initialized;
            }
            else if (!ctx->proto.initialize) {
                ctx->inflag |= InternalFlag::init_succeed;
            }
            if (err == nullptr && !any(ctx->inflag & InternalFlag::init_succeed)) {
                err = consterror{"already failed to initialize"};
            }
            return err;
        }

        bool STDCALL uninitialize(Stopper stop, CNet* ctx) {
            if (!any(ctx->inflag & InternalFlag::uninitialized) && ctx->proto.uninitialize) {
                ctx->proto.uninitialize(stop, ctx, ctx->user);
                ctx->inflag |= InternalFlag::uninitialized;
            }
            return true;
        }

        Error STDCALL write(Stopper stop, CNet* ctx, const char* data, size_t size, size_t* written) {
            if (!ctx || !written) {
                return consterror{"ctx or written is nullptr"};
            }
            if (auto err = initialize(stop, ctx); err != nullptr) {
                return err;
            }
            auto write_buf = [&](Buffer<const char>* buf) {
                WRITE_START:
                    auto result = ctx->proto.write(stop, ctx, ctx->user, buf);
                    if (result && any(ctx->flag & CNetFlag::repeat_writing) && buf->size != buf->proced) {
                        goto WRITE_START;
                    }
                    return result;
            };
            auto write_impl = [&](Buffer<const char>* buf) -> Error {
                auto with_make_buf = [&](auto&& cb) -> Error {
                    return cb(buf);
                };
                if (ctx->proto.write) {
                    return with_make_buf(write_buf);
                }
                else if (ctx->next) {
                    return with_make_buf(
                        [&](Buffer<const char>* buf) {
                            return write(stop, ctx->next, buf->ptr, buf->size, &buf->proced);
                        });
                }
                return consterror{"failed to write buffer"};
            };
            Buffer<const char> buf;
            buf.ptr = data;
            buf.size = size;
            buf.proced = 0;
            auto err = write_impl(&buf);
            if (err == nullptr) {
                *written = buf.proced;
                return nil();
            }
            return err;
        }

        Error STDCALL read(Stopper stop, CNet* ctx, char* buffer, size_t size, size_t* red) {
            if (!ctx || !red) {
                return consterror{"cnet: ctx or red is nullptr"};
            }
            auto err = initialize({}, ctx);
            if (err != nullptr) {
                return err;
            }
            Buffer<char> buf;
            if (ctx->proto.read) {
                buf.ptr = buffer;
                buf.size = size;
                buf.proced = 0;
                auto err = ctx->proto.read(stop, ctx, ctx->user, &buf);
                if (err == nullptr) {
                    *red = buf.proced;
                    return nil();
                }
                return err;
            }
            else if (ctx->next) {
                return read(stop, ctx->next, buffer, size, red);
            }
            return consterror{"cnet: no read is set"};
        }

        bool STDCALL set_number(CNet* ctx, std::int64_t key, std::int64_t value) {
            if (!ctx || !ctx->proto.settings_number) {
                return false;
            }
            return ctx->proto.settings_number(ctx, ctx->user, key, value);
        }

        bool STDCALL set_ptr(CNet* ctx, std::int64_t key, void* value) {
            if (!ctx || !ctx->proto.settings_ptr) {
                return false;
            }
            return ctx->proto.settings_ptr(ctx, ctx->user, key, value);
        }

        std::int64_t STDCALL query_number(CNet* ctx, std::int64_t key) {
            if (!ctx || !ctx->proto.query_number) {
                return 0;
            }
            return ctx->proto.query_number(ctx, ctx->user, key);
        }

        void* STDCALL query_ptr(CNet* ctx, std::int64_t key) {
            if (!ctx || !ctx->proto.query_ptr) {
                return nullptr;
            }
            return ctx->proto.query_ptr(ctx, ctx->user, key);
        }

        bool STDCALL is_supported(CNet* ctx, std::int64_t key, bool ptr) {
            if (!ctx) {
                return false;
            }
            if (!ctx->proto.is_support) {
                return false;
            }
            if (ptr && !ctx->proto.query_ptr) {
                return false;
            }
            if (!ptr && !ctx->proto.query_number) {
                return false;
            }
            return ctx->proto.is_support(key, ptr);
        }

        void STDCALL delete_cnet(CNet* ctx) {
            delete ctx;
        }
    }  // namespace cnet
}  // namespace utils
