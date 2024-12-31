/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/error.h>
#include <helper/defer.h>
#include <unicode/utf/convert.h>
#include <fnet/dll/errno.h>
#include <platform/detect.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <string.h>
#endif
namespace futils {
    namespace fnet::error {
        struct Defined {
            NumErrMode mode = NumErrMode::use_default;
            void (*fn)(helper::IPushBacker<> pb, std::uint64_t code) = nullptr;
        };
        struct Record {
            Category categ = Category::none;
            Defined def;
        };

        static Defined defined_error[3];

        constexpr auto storage_size = 255;

        static Record spec_record[storage_size];
        std::uint32_t count = 0;

        static int convert_index(Category categ) {
            switch (categ) {
                case Category::os:
                    return 0;
                case Category::lib:
                    return 1;
                case Category::app:
                    return 2;
                default:
                    return -1;
            }
        }

        fnet_dll_implement(bool) register_categspec_nummsg(Category categ, NumErrMode mode, void (*fn)(helper::IPushBacker<> pb, std::uint64_t code)) {
            if (categ == Category::none || !fn) {
                return false;
            }
            if (auto index = convert_index(categ); index >= 0) {
                defined_error[index].mode = mode;
                defined_error[index].fn = fn;
                return true;
            }
            for (auto i = 0; i < count; i++) {
                if (spec_record[i].categ == categ) {
                    spec_record[i].def.mode = mode;
                    spec_record[i].def.fn = fn;
                    return true;
                }
            }
            if (count >= storage_size) {
                return false;
            }
            auto index = count++;
            spec_record[index].categ = categ;
            spec_record[index].def.mode = mode;
            spec_record[index].def.fn = fn;
            return true;
        }

        namespace internal {
            auto fetch_categ(Category categ, auto&& cb) {
                if (auto index = convert_index(categ); index >= 0) {
                    return cb(defined_error[index]);
                }
                for (auto i = 0; i < count; i++) {
                    if (spec_record[i].categ == categ) {
                        return cb(spec_record[i].def);
                    }
                }
                return cb(Defined{});
            }

            fnet_dll_export(void) invoke_categspec(helper::IPushBacker<> pb, Category categ, std::uint64_t val) {
                fetch_categ(categ, [&](Defined def) {
                    if (def.fn) {
                        def.fn(pb, val);
                    }
                });
            }

            fnet_dll_export(NumErrMode) categspec_mode(Category categ) {
                return fetch_categ(categ, [&](Defined def) {
                    return def.mode;
                });
            }

#ifdef FUTILS_PLATFORM_WINDOWS

            static auto d = helper::init([]() {
                register_categspec_nummsg(Category::os, NumErrMode::use_custom, [](helper::IPushBacker<> pn, std::uint64_t code) {
                    LPWSTR lpMsgBuf;
                    FormatMessageW(
                        FORMAT_MESSAGE_ALLOCATE_BUFFER |
                            FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS |
                            FORMAT_MESSAGE_MAX_WIDTH_MASK,
                        NULL,
                        code,
                        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                        (LPWSTR)&lpMsgBuf,
                        0, NULL);
                    const auto d = helper::defer([&] {
                        LocalFree(lpMsgBuf);
                    });
                    strutil::append(pn, "code=");
                    number::to_string(pn, code);
                    strutil::append(pn, " ");
                    utf::convert<2, 1>(lpMsgBuf, pn);
                });
            });
#else

            struct StrHolder {
               private:
                const char *buf;
                size_t size;
                bool allocated;

               public:
                StrHolder(const char *buf, size_t size, bool allocated)
                    : buf(buf), size(size), allocated(allocated) {}

                ~StrHolder() {
                    if (allocated) {
                        free_normal(const_cast<char *>(buf), DNET_DEBUG_MEMORY_LOCINFO(true, size, 1));
                    }
                }

                const char *c_str() const {
                    return buf;
                }

                StrHolder(StrHolder &&other)
                    : buf(std::exchange(other.buf, nullptr)),
                      size(std::exchange(other.size, 0)),
                      allocated(std::exchange(other.allocated, false)) {}
            };

            template <class R, class... Arg>
            StrHolder strerror_wrap(R (*strerr)(Arg...), int n) noexcept {
                size_t size = 1024;
                auto buf = static_cast<char *>(alloc_normal(size, 1, DNET_DEBUG_MEMORY_LOCINFO(true, size, 1)));
                if (!buf) {
                    return StrHolder("<malloc for strerror failed>", 0, false);
                }
                if constexpr (std::is_convertible_v<R, int>) {
                    // XSI compatible
                    for (;;) {
                        auto r = strerr(n, buf, size);
                        if (r == -1 && errno == ERANGE) {
                            size *= 2;
                            auto new_buf = static_cast<char *>(realloc_normal(buf, size, 1, DNET_DEBUG_MEMORY_LOCINFO(true, size, 1)));
                            if (!new_buf) {
                                free_normal(buf, DNET_DEBUG_MEMORY_LOCINFO(true, size, 1));
                                return StrHolder("<realloc for strerror failed>", 0, false);
                            }
                            buf = new_buf;
                            continue;
                        }
                        else if (r == -1) {
                            free_normal(buf, DNET_DEBUG_MEMORY_LOCINFO(true, size, 1));
                            if (errno == EINVAL) {
                                return StrHolder("invalid error number", 0, false);
                            }
                            return StrHolder("<strerror failed>", 0, false);
                        }
                        else {
                            return StrHolder(buf, size, true);
                        }
                    }
                }
                else {
                    // GNU compatible
                    for (;;) {
                        errno = 0;
                        auto r = strerr(n, buf, size);
                        if (r != buf) {
                            free_normal(buf, DNET_DEBUG_MEMORY_LOCINFO(true, size, 1));
                            return StrHolder(r, 0, false);
                        }
                        else if (errno == 0) {
                            return StrHolder(buf, size, true);
                        }
                        else if (errno == ERANGE) {
                            size *= 2;
                            auto new_buf = static_cast<char *>(realloc_normal(buf, size, 1, DNET_DEBUG_MEMORY_LOCINFO(true, size, 1)));
                            if (!new_buf) {
                                // at GNU, string error is omitted but appear with null terminated
                                // so use omited error message
                                return StrHolder(buf, size, true);
                            }
                            buf = new_buf;
                            continue;
                        }
                        else {
                            auto err = errno;
                            free_normal(buf, DNET_DEBUG_MEMORY_LOCINFO(true, size, 1));
                            if (err == EINVAL) {
                                return StrHolder("invalid error number", 0, false);
                            }
                            return StrHolder("<strerror failed>", 0, false);
                        }
                    }
                }
            }

            StrHolder all_strerror(int n) {
                return strerror_wrap(strerror_r, n);
            }

            static auto d = helper::init([]() {
                register_categspec_nummsg(Category::os, NumErrMode::use_custom, [](helper::IPushBacker<> pn, std::uint64_t code) {
                    auto res = all_strerror(code);
                    strutil::append(pn, "code=");
                    number::to_string(pn, code);
                    strutil::append(pn, " ");
                    strutil::append(pn, res.c_str());
                });
            });
#endif

        }  // namespace internal
        fnet_dll_implement(Error) Errno() {
            return Error(get_error(), Category::os);
        }
    }  // namespace fnet::error
}  // namespace futils
