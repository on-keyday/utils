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

namespace utils {
    namespace cnet {

        // cnet context
        struct CNet;

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
            bool (*initialize)(CNet* ctx, void* user);
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
            void (*initialize)(CNet* ctx, T* user);
            bool (*write)(CNet* ctx, T* user, Buffer<const char> buf);
            bool (*read)(CNet* ctx, T* user, Buffer<char> buf);
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
        DLL bool STDCALL set_lowlevel_protocol(CNet* ctx, CNet* protocol);

        // get lowlevel protocol
        DLL CNet* STDCALL get_lowlevel_protocol(CNet* ctx);

        // write to context
        DLL bool STDCALL write(CNet* ctx, const char* data, size_t size, size_t* written);

        // read from context
        DLL bool STDCALL read(CNet* ctx, char* buffer, size_t size, size_t* red);

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

    }  // namespace cnet
}  // namespace utils
