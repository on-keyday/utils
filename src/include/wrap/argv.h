/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// argv - wrap argv to utf8
// need to link libutils
#pragma once
#include "../platform/windows/dllexport_header.h"
#include "../unicode/utf/convert.h"
#include "light/string.h"
#include "light/vector.h"

namespace utils {
    namespace wrap {
        template <class Char = char, class String = wrap::string, template <class...> class Vector = wrap::vector>
        struct ArgvVector {
            Vector<Char*> argv_;
            Vector<String> holder_;

            template <class T>
            bool translate(T&& t) {
                for (auto&& v : t) {
                    if (!push_back(v)) {
                        return false;
                    }
                }
                return true;
            }

            template <class Begin, class End>
            bool translate(Begin begin, End end) {
                for (auto i = begin; i != end; i++) {
                    if (!push_back(*i)) {
                        return false;
                    }
                }
                return true;
            }

            template <class V>
            bool push_back(V&& v) {
                String result;
                if (!utf::convert(v, result)) {
                    return false;
                }
                holder_.push_back(std::move(result));
                return true;
            }

            void clear() {
                argv_.clear();
                holder_.clear();
            }

            template <class Int = int>
            void arg(Int& argc, Char**& argv) {
                argv_.clear();
                for (size_t i = 0; i < holder_.size(); i++) {
                    argv_.push_back(const_cast<Char*>(holder_[i].c_str()));
                }
                argv_.push_back(nullptr);
                argv = argv_.data();
                argc = static_cast<Int>(holder_.size());
            }
        };

        struct DLL U8Arg {
#ifdef _WIN32
           private:
            ArgvVector<> replaced;
            int* argcplace = nullptr;
            char*** argvplace = nullptr;
            int argcvalue = 0;
            char** argvvalue = nullptr;

           public:
            explicit U8Arg(int& argc, char**& argv);

            ~U8Arg();
#else
            explicit constexpr U8Arg(int&, char**&) {}
            constexpr ~U8Arg() {}
#endif
        };

    }  // namespace wrap
}  // namespace utils
