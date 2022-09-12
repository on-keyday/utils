/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// httpstring - http string
#pragma once
#include "dll/dllh.h"
#include "dll/httpbuf.h"

namespace utils {
    namespace dnet {
        struct dnet_class_export String {
           private:
            HTTPBuf buf;

           public:
            constexpr String() = default;
            constexpr String(HTTPBuf&& buf)
                : buf(std::move(buf)) {}
            constexpr String(String&&) = default;
            String& operator=(String&&) = default;
            String(const char* val);
            String(const char* val, size_t size);

            String(const String&);
            String& operator=(const String&);

            char& operator[](size_t i);
            const char& operator[](size_t i) const;

            size_t size() const;
            size_t& refsize();
            size_t cap() const;

            bool resize(size_t i);
            bool recap(size_t i);

            void push_back(char c);

            bool operator==(const String&) const;
            bool operator<(const String&) const;

            const char* begin() const;
            const char* end() const;

            const char* c_str();
            char* text();

            bool shift(size_t len);

            constexpr HTTPBuf& get() {
                return buf;
            }
        };

        struct BorrowString {
            HTTPBuf& from;
            String& to;
            constexpr BorrowString(HTTPBuf& from, String& to)
                : from(from), to(to) {
                to = std::move(from);
            }

            constexpr ~BorrowString() {
                from = std::move(to.get());
            }
        };
    }  // namespace dnet
}  // namespace utils
