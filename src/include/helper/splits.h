/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// splits - split method
#pragma once

#include "../core/sequencer.h"

#include "../wrap/lite/string.h"
#include "../wrap/lite/vector.h"

namespace utils {
    namespace helper {
        template <class T, class String, class Sep, template <class...> class Vec>
        void split(Vec<String>& result, Sequencer<T>& input, Sep&& sep, size_t n = ~0, bool igafter = false) {
            String current;
            size_t count = 0;
            while (!input.eos()) {
                if (count < n && input.seek_if(sep)) {
                    result.push_back(std::move(current));
                    count++;
                    if (count >= n && igafter) {
                        break;
                    }
                    continue;
                }
                current.push_back(input.current());
                input.consume();
            }
            if (!igafter) {
                result.push_back(std::move(current));
            }
        }

        template <class T, class String, template <class...> class Vec>
        void lines(Vec<String>& result, Sequencer<T>& input, bool needtk = false) {
            String current;
            while (!input.eos()) {
                if (input.seek_if("\r\n")) {
                    if (needtk) {
                        current.push_back('\r');
                        current.push_back('\n');
                    }
                    result.push_back(std::move(current));
                }
                else if (input.seek_if("\r")) {
                    if (needtk) {
                        current.push_back('\r');
                    }
                    result.push_back(std::move(current));
                }
                else if (input.seek_if("\n")) {
                    if (needtk) {
                        current.push_back('\n');
                    }
                    result.push_back(std::move(current));
                }
                else {
                    current.push_back(input.current());
                    input.consume();
                }
            }
            if (current.size()) {
                result.push_back(std::move(current));
            }
            return true;
        }

        template <class String = wrap::string, template <class...> class Vec = wrap::vector, class T, class Sep>
        Vec<String> split(T&& input, Sep&& sep, size_t n = ~0, bool igafter = false) {
            Vec<String> result;
            Sequencer<typename BufferType<T&>::type> intmp(input);
            split(result, intmp, sep, n, igafter);
            return result;
        }

        template <class String = wrap::string, template <class...> class Vec = wrap::vector, class T>
        Vec<String> lines(T&& input, bool needtk = false) {
            Vec<String> result;
            Sequencer<typename BufferType<T&>::type> intmp(input);
            lines(result, intmp, needtk);
            return result;
        }
    }  // namespace helper
}  // namespace utils
