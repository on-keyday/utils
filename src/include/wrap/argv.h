/*license*/

// argv - wrap argv to utf8
#pragma once

#include "../utf/convert.h"
#include "lite/string.h"
#include "lite/vector.h"

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
                for (size_t i = 0; i < argv_.size(); i++) {
                    argv_.push_back(const_cast<Char*>(holder_[i].c_str()));
                }
                argv_.push_back(nullptr);
                argv = argv_.data();
                argc = static_cast<Int>(holder_.size());
            }
        };

        struct U8Arg {
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
            constexpr U8Argv(int&, char**&) {}
            constexpr ~U8Argv() {}
#endif
        };

    }  // namespace wrap
}  // namespace utils
