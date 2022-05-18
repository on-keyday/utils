/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cnet - c language style network function
#pragma once
#include "../platform/windows/dllexport_header.h"
#include "../wrap/light/enum.h"
#include "../helper/equal.h"
#include "../iface/base_iface.h"
#include "../helper/appender.h"

namespace utils {
    namespace cnet {

        // cnet context
        struct CNet;

        // error interface like golang
        enum class Kind {
            note,
            os,
            openssl,
            should_retry,
            wrap,
            fatal,
        };

        constexpr const char* default_kinds[] = {
            "note",
            "os",
            "openssl",
            "should_retry",
            "wrap",
            "fatal",
        };

        constexpr auto kind_c(Kind kind) {
            return default_kinds[int(kind)];
        }

        struct Error : iface::Error<iface::Powns, Error> {
            using iface::Error<iface::Powns, Error>::Error;
            bool as(Kind kind) const {
                return kind_of(kind_c(kind));
            }
        };

        inline Error nil() {
            return {};
        }

        using Stopper = iface::Stopper<iface::Ref, Error>;

        using pushbacker = iface::PushBacker<iface::Ref>;

        struct wraperror {
            iface::Subscript<iface::Sized<iface::Powns>> now;
            Error err;
            void error(pushbacker pb) {
                helper::appends(pb, now, ": ");
                err.error(pb);
            }

            bool kind_of(const char* k) {
                return helper::equal(k, kind_c(Kind::wrap));
            }

            Error unwrap() {
                return std::move(err);
            }
        };

        struct consterror {
            const char* msg;
            void error(pushbacker pb) {
                helper::append(pb, msg);
            }
            bool kind_of(const char* k) {
                return helper::equal(k, kind_c(Kind::fatal));
            }
        };

        enum class CNetFlag {
            none = 0,
            // no more link is required
            final_link = 0x1,
            // delay initialization until I/O start
            init_before_io = 0x2,
            // if write data and make_data exists
            // must call make_data
            must_make_data = 0x4,
            // repeat writing while proced != size
            repeat_writing = 0x8,
            // make it able to re-initialize after uninitialized
            reinitializable = 0x10,
            // once set no delete link
            once_set_no_delete_link = 0x20,
        };

        DEFINE_ENUM_FLAGOP(CNetFlag)

        template <class Char>
        struct Buffer {
            Char* ptr;
            size_t size;
            size_t proced;
        };

        struct MadeBuffer {
            Buffer<char> buf;
            void (*deleter)(Buffer<char> buf);
            ~MadeBuffer() {
                if (deleter) {
                    deleter(buf);
                }
            }
        };

        struct Protocol {
            Error (*initialize)(Stopper stop, CNet* ctx, void* user);
            bool (*write)(CNet* ctx, void* user, Buffer<const char>* buf);
            bool (*read)(CNet* ctx, void* user, Buffer<char>* buf);
            bool (*make_data)(CNet* ctx, void* user, MadeBuffer* buf, Buffer<const char>* input);
            void (*uninitialize)(CNet* ctx, void* user);

            bool (*settings_number)(CNet* ctx, void* user, std::int64_t key, std::int64_t value);
            bool (*settings_ptr)(CNet* ctx, void* user, std::int64_t key, void* value);
            std::int64_t (*query_number)(CNet* ctx, void* user, std::int64_t key);
            void* (*query_ptr)(CNet* ctx, void* user, std::int64_t key);
            bool (*is_support)(std::int64_t key, bool ptr);
            void (*deleter)(void* user);
        };

        template <class T, class SettingValue = void>
        struct ProtocolSuite {
            Error (*initialize)(Stopper stop, CNet* ctx, T* user);
            bool (*write)(CNet* ctx, T* user, Buffer<const char>* buf);
            bool (*read)(CNet* ctx, T* user, Buffer<char>* buf);
            bool (*make_data)(CNet* ctx, T* user, MadeBuffer* buf, Buffer<const char>* input);
            void (*uninitialize)(CNet* ctx, T* user);

            bool (*settings_number)(CNet* ctx, T* user, std::int64_t key, std::int64_t value);
            bool (*settings_ptr)(CNet* ctx, T* user, std::int64_t key, SettingValue* value);
            std::int64_t (*query_number)(CNet* ctx, T* user, std::int64_t key);
            void* (*query_ptr)(CNet* ctx, T* user, std::int64_t key);
            bool (*is_support)(std::int64_t key, bool ptr);
            void (*deleter)(T* user);
        };

        // create context
        DLL CNet* STDCALL create_cnet(CNetFlag flag, void* user, const Protocol* proto);

        // create context for typed function
        template <class T>
        CNet* create_cnet(CNetFlag flag, T* user, ProtocolSuite<T> proto) {
            return create_cnet(flag, static_cast<T*>(user), reinterpret_cast<const Protocol*>(&proto));
        }

        // reset protocol
        DLL bool STDCALL reset_protocol_context(CNet* ctx, CNetFlag flag, void* user, const Protocol* proto);

        // reset protocol for typed function
        template <class T>
        bool reset_protocol_context(CNet* ctx, T* user, ProtocolSuite<T> proto) {
            return reset_protocol_context(ctx, static_cast<void*>(user), reinterpret_cast<const Protocol*>(&proto));
        }

        // set lowlevel protocol
        // ownership of protocol is transferred if succeed
        DLL bool STDCALL set_lowlevel_protocol(CNet* ctx, CNet* protocol);

        // get lowlevel protocol
        DLL CNet* STDCALL get_lowlevel_protocol(CNet* ctx);

        // set user defined callback
        DLL bool STDCALL set_callback(CNet* ctx, bool (*cb)(CNet*, void*), void* data);

        template <class Fn>
        bool set_lambda(CNet* ctx, Fn& fn) {
            using pointer = decltype(std::addressof(fn));
            return set_callback(
                ctx, [](CNet* ctx, void* cpt) {
                    auto res = (*pointer(cpt))(ctx);
                    return bool(res);
                },
                std::addressof(fn));
        }

        // invoke user defined callback
        DLL bool STDCALL invoke_callback(CNet* ctx);

        // initialize protocol context
        DLL Error STDCALL initialize(Stopper stop, CNet* ctx);

        // open connection
        inline Error open(CNet* ctx) {
            return initialize({}, ctx);
        }

        // uninititalize protocol context
        DLL bool STDCALL uninitialize(CNet* ctx);

        // close connection
        inline bool close(CNet* ctx) {
            return uninitialize(ctx);
        }

        // write to context
        DLL bool STDCALL write(CNet* ctx, const char* data, size_t size, size_t* written);

        // do protocol request
        inline bool request(CNet* ctx, size_t* written) {
            return write(ctx, nullptr, 0, written);
        }

        // read from context
        DLL bool STDCALL read(CNet* ctx, char* buffer, size_t size, size_t* red);

        // receive protocol response
        inline bool response(CNet* ctx, size_t* red) {
            return read(ctx, nullptr, 0, red);
        }

        // set number setting
        DLL bool STDCALL set_number(CNet* ctx, std::int64_t key, std::int64_t value);

        // set pointer setting
        DLL bool STDCALL set_ptr(CNet* ctx, std::int64_t key, void* value);

        // get number setting
        DLL std::int64_t STDCALL query_number(CNet* ctx, std::int64_t key);

        // get pointer setting
        DLL void* STDCALL query_ptr(CNet* ctx, std::int64_t key);

        // is this key surpported ?
        DLL bool STDCALL is_supported(CNet* ctx, std::int64_t key, bool ptr);

        // delete cnet context
        DLL void STDCALL delete_cnet(CNet* ctx);

        enum CNetCommonSetting {
            protocol_name = 0,  // protocol name (constant value)
            protocol_type = 1,  // protocol type (constant value)
            error_code = 2,     // most recent error code
            user_defined_start = 100,
        };

        enum ProtocolType {
            unknown = 0,
            byte_stream = 1,
            block_stream = 2,
            request_response = 3,
            conn_acceptor = 4,
            polling = 5,
        };

        inline const char* get_protocol_name(CNet* ctx) {
            return (const char*)query_ptr(ctx, protocol_name);
        }

        inline ProtocolType get_protocol_type(CNet* ctx) {
            return (ProtocolType)query_number(ctx, protocol_type);
        }

        inline std::int64_t get_error(CNet* ctx) {
            return query_number(ctx, error_code);
        }

        inline bool protocol_is(CNet* ctx, const char* required) {
            return helper::equal(get_protocol_name(ctx), required);
        }

    }  // namespace cnet
}  // namespace utils
