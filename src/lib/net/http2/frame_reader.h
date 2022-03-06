/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// frame_reader - http2 frame reader/writer
#pragma once
#include "../../../include/net/http2/error_type.h"
#include "../../../include/endian/endian.h"
#include "../../../include/wrap/lite/string.h"

namespace utils {
    namespace net {
        namespace http2 {
            namespace internal {
                struct FrameWriter {
                    wrap::string str;
                    template <class T>
                    void write(T& val, size_t size = sizeof(T)) {
                        auto nt = endian::to_network(&val);
                        char* ptr = reinterpret_cast<char*>(&nt);
                        for (auto i = sizeof(T) - size; i < sizeof(T); i++) {
                            str.push_back(ptr[i]);
                        }
                    }

                    void write(const wrap::string& val, size_t sz) {
                        str.append(val, 0, sz);
                    }
                    void write(const Dummy&) {}
                };

                template <class Str = wrap::string>
                struct FrameReader {
                    Str ref;
                    size_t pos = 0;

                    void tidy() {
                        ref.erase(0, pos);
                        pos = 0;
                    }

                    bool read(Dummy&) {
                        return true;
                    }

                    template <class T>
                    bool read(T& t, size_t size = sizeof(T)) {
                        if (size > sizeof(T)) {
                            return false;
                        }
                        T cvt = T{};
                        char* ptr = reinterpret_cast<char*>(std::addressof(cvt));
                        for (size_t i = sizeof(T) - size; i < sizeof(T); i++) {
                            if (pos >= ref.size()) {
                                return false;
                            }
                            ptr[i] = ref[pos];
                            pos++;
                        }
                        t = endian::from_network(&cvt);
                        return true;
                    }

                    bool read(wrap::string& str, size_t size) {
                        if (ref.size() - pos < size) {
                            return false;
                        }
                        auto first = ref.begin() + pos;
                        auto second = ref.begin() + pos + size;
                        str.append(first, second);
                        pos += size;
                        return true;
                    }
                };

            }  // namespace internal
        }      // namespace http2
    }          // namespace net
}  // namespace utils
