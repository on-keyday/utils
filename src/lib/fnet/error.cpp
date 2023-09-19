/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/error.h>
#include <helper/defer.h>
#include <unicode/utf/convert.h>
#include <fnet/dll/errno.h>
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <Windows.h>
#endif
namespace utils {
    namespace fnet::error {
        struct Defined {
            NumErrMode mode = NumErrMode::use_default;
            void (*fn)(helper::IPushBacker<> pb, std::uint64_t code) = nullptr;
        };
        struct Record {
            ErrorCategory categ = ErrorCategory::noerr;
            Defined def;
        };

        static Defined defined_error[9];

        constexpr auto storage_size = 255;

        static Record spec_record[storage_size];
        std::uint32_t count = 0;

        static int convert_index(ErrorCategory categ) {
            switch (categ) {
                case ErrorCategory::syserr:
                    return 0;
                case ErrorCategory::liberr:
                    return 1;
                case ErrorCategory::sslerr:
                    return 2;
                case ErrorCategory::cryptoerr:
                    return 3;
                case ErrorCategory::fneterr:
                    return 4;
                case ErrorCategory::quicerr:
                    return 5;
                case ErrorCategory::apperr:
                    return 6;
                case ErrorCategory::internalerr:
                    return 7;
                case ErrorCategory::validationerr:
                    return 8;
                default:
                    return -1;
            }
        }

        fnet_dll_implement(bool) register_categspec_nummsg(ErrorCategory categ, NumErrMode mode, void (*fn)(helper::IPushBacker<> pb, std::uint64_t code)) {
            if (categ == ErrorCategory::noerr || !fn) {
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
            auto fetch_categ(ErrorCategory categ, auto&& cb) {
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

            fnet_dll_export(void) invoke_categspec(helper::IPushBacker<> pb, ErrorCategory categ, std::uint64_t val) {
                fetch_categ(categ, [&](Defined def) {
                    if (def.fn) {
                        def.fn(pb, val);
                    }
                });
            }

            fnet_dll_export(NumErrMode) categspec_mode(ErrorCategory categ) {
                return fetch_categ(categ, [&](Defined def) {
                    return def.mode;
                });
            }

#ifdef UTILS_PLATFORM_WINDOWS

            static auto d = helper::init([]() {
                register_categspec_nummsg(ErrorCategory::syserr, NumErrMode::use_nothing, [](helper::IPushBacker<> pn, std::uint64_t code) {
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
#endif

        }  // namespace internal
        fnet_dll_implement(Error) Errno() {
            return Error(get_error(), ErrorCategory::syserr);
        }
    }  // namespace fnet::error
}  // namespace utils
