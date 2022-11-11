/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/http2/h2state.h>
#include <dnet/dll/httpbufproxy.h>
#include <dnet/dll/glheap.h>
#include <dnet/http2/http2.h>
#include <helper/equal.h>
#include <helper/view.h>
#include <cstring>

namespace utils {
    namespace dnet {

        char& String::operator[](size_t i) {
            HTTPBufProxy p{buf};
            return p.text()[i];
        }

        const char& String::operator[](size_t i) const {
            HTTPConstBufProxy p{buf};
            return p.text()[i];
        }

        size_t String::size() const {
            HTTPConstBufProxy p{buf};
            return p.size();
        }

        void String::push_back(char c) {
            HTTPBufProxy p{buf};
            if (!p.text()) {
                p.inisize(10);
                if (!p.text()) {
                    return;
                }
            }
            HTTPPushBack pb{&buf};
            pb.push_back(c);
        }

        bool String::operator==(const String& v) const {
            return helper::equal(*this, v);
        }

        const char* String::begin() const {
            HTTPConstBufProxy p{buf};
            return p.text();
        }

        const char* String::end() const {
            HTTPConstBufProxy p{buf};
            return p.text() + p.size();
        }

        const char* String::c_str() {
            HTTPConstBufProxy p{buf};
            if (!p.text()) {
                return nullptr;
            }
            p.text()[p.size()] = 0;
            return p.text();
        }

        String::String(const String& right) {
            helper::append(*this, right);
        }

        String& String::operator=(const String& right) {
            if (this != &right) {
                return *this;
            }
            HTTPBufProxy p{buf};
            p.size() = 0;
            helper::append(*this, right);
            return *this;
        }

        bool String::operator<(const String& v) const {
            return this->size() < v.size();
        }

        String::String(const char* val) {
            helper::append(*this, val);
        }

        String::String(const char* val, size_t size) {
            helper::append(*this, helper::SizedView(val, size));
        }

        size_t String::cap() const {
            HTTPConstBufProxy p{buf};
            return p.cap();
        }

        bool String::resize(size_t s) {
            HTTPBufProxy p{buf};
            if (p.cap() > s) {
                p.size() = s;
                return true;
            }
            if (!recap(s + 2)) {
                return false;
            }
            p.size() = s;
            return true;
        }

        char* String::text() {
            HTTPBufProxy p{buf};
            return p.text();
        }

        size_t& String::refsize() {
            HTTPBufProxy p{buf};
            return p.size();
        }

        bool String::recap(size_t i) {
            HTTPBufProxy p{buf};
            if (!p.text()) {
                p.text() = get_cvec(i, DNET_DEBUG_MEMORY_LOCINFO(true, i, alignof(char)));
                if (!p.text()) {
                    return false;
                }
            }
            else {
                if (!resize_cvec(p.text(), i, DNET_DEBUG_MEMORY_LOCINFO(true, p.cap(), alignof(char)))) {
                    return false;
                }
            }
            p.cap() = i;
            if (i < p.size()) {
                p.size() = i;
            }
            return true;
        }

        bool String::shift(size_t len) {
            HTTPBufProxy p{buf};
            if (p.size() < len) {
                return false;
            }
            memmove(p.text(), p.text() + len, p.size() - len);
            p.size() -= len;
            return true;
        }

        bool String::append(const char* data, size_t size) {
            if (!data || !size) {
                return false;
            }
            auto cursize = buf.getsize();
            if (cursize + size < cursize) {
                return false;
            }
            if (!resize(cursize + size)) {
                return false;
            }
            memmove(text() + cursize, data, size);
            return true;
        }

    }  // namespace dnet
}  // namespace utils
