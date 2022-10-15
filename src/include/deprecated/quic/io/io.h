/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// io - io definition
#pragma once
#include "../doc.h"
#include "../core/event.h"
#include "../mem/bytes.h"

namespace utils {
    namespace quic {
        namespace io {

            using TargetKey = uint;
            using TargetID = tsize;

            struct IPAddress {
                byte addr[16];
                byte& operator[](tsize i) {
                    return addr[i];
                }
                const byte& operator[](tsize i) const {
                    return addr[i];
                }
            };

            using Port = ushort;
            using OptionKey = tsize;

            constexpr TargetID global = 0;
            constexpr TargetID invalid = ~0;

            struct Option {
                OptionKey key;
                TargetID target;
                union {
                    void* ptr;
                    const byte* b;
                };
                union {
                    void* rp;
                    tsize rn;
                };
            };

            struct Target {
                TargetKey key;
                TargetID target;
                union {
                    struct {
                        IPAddress address;
                        Port port;
                    } ip;
                    struct {
                        const byte* address;
                        tsize len;
                    } local;
                };
                constexpr Target()
                    : ip({}) {}
            };

            constexpr Target IPv6(IPAddress addr, ushort port) {
                Target t;
                t.ip.address = addr;
                t.ip.port = port;
                return t;
            }

            constexpr Target IPv4(byte b1, byte b2, byte b3, byte b4, ushort port) {
                Target t;
                t.ip.address[0] = b1;
                t.ip.address[1] = b2;
                t.ip.address[2] = b3;
                t.ip.address[3] = b4;
                t.ip.port = port;
                return t;
            }

            enum class Status {
                ok,
                incomplete,
                failed,
                fatal,
                canceled,
                timeout,
                unsupported,
                invalid_argument,
                too_large_data,
                resource_exhausted,
            };

            using ErrorCode = tsize;

            struct Result {
                Status status;
                union {
                    ErrorCode code;
                    TargetID target;
                    tsize len;
                };
                bool nsys;
            };

            constexpr bool ok(Result s) {
                return s.status == Status::ok;
            }

            constexpr bool incomplete(Result s) {
                return s.status == Status::incomplete;
            }

            constexpr bool fail(Result s) {
                return s.status == Status::failed || s.status == Status::canceled ||
                       s.status == Status::timeout;
            }

            constexpr bool timeout(Result s) {
                return s.status == Status::timeout;
            }

            constexpr bool unsupported(Result s) {
                return s.status == Status::unsupported;
            }

            constexpr bool large_data(Result s) {
                return s.status == Status::too_large_data;
            }

            constexpr ErrorCode error_code(Result s) {
                return s.code;
            }

            constexpr TargetID target_id(Result s) {
                return s.target;
            }

            constexpr tsize length(Result s) {
                return s.len;
            }

            constexpr bool non_sys(Result s) {
                return s.nsys;
            }

            using Poller = void (*)(void* user, Target* from, const byte* data, tsize len);

            struct IO {
               private:
                void* C;
                Result (*option_f)(void* C, io::Option* opt);
                Result (*write_to_f)(void* C, const Target* target, const byte* data, tsize len);
                Result (*read_from_f)(void* C, Target* target, byte* data, tsize len);
                Result (*new_target_f)(void* C, const Target* hint, Mode mode);
                Result (*select_f)(void* C, io::Poller callback);
                Result (*discard_target_f)(void* C, TargetID id);
                void (*free_f)(void* C);

                static Result unsupport() {
                    return {Status::unsupported, invalid, true};
                }

               public:
                Result option(io::Option* opt) {
                    if (option_f) {
                        return option_f(C, opt);
                    }
                    return unsupport();
                }

                Result write_to(const Target* target, const byte* data, tsize len) {
                    if (write_to_f) {
                        return write_to_f(C, target, data, len);
                    }
                    return unsupport();
                }

                Result read_from(Target* target, byte* data, tsize len) {
                    if (read_from_f) {
                        return read_from_f(C, target, data, len);
                    }
                    return unsupport();
                }

                Result new_target(const Target* hint, Mode mode) {
                    if (new_target_f) {
                        return new_target_f(C, hint, mode);
                    }
                    return unsupport();
                }

                Result select(io::Poller poller) {
                    if (select_f) {
                        return select_f(C, poller);
                    }
                    return unsupport();
                }

                Result discard_target(TargetID id) {
                    if (discard_target_f) {
                        return discard_target_f(C, id);
                    }
                    return unsupport();
                }

                void free() {
                    if (free_f) {
                        free_f(C);
                    }
                    *this = {};
                }
            };

            template <class T>
            struct IOComponent {
                T* C;
                Result (*option)(T* C, io::Option* opt);
                Result (*write_to)(T* C, const Target* target, const byte* data, tsize len);
                Result (*read_from)(T* C, Target* target, byte* data, tsize len);
                Result (*new_target)(T* C, const Target* hint, Mode mode);
                Result (*select)(T* C, io::Poller callback);
                Result (*discard_target)(T* C, TargetID id);
                void (*free)(T* C);
                IO& to_IO() {
                    return reinterpret_cast<IO&>(*this);
                }
            };

            struct CommonIOWait {
                io::Target target;
                bytes::Bytes b;
                core::Event next;
                Result res;
            };

            struct Timeout {
                std::time_t until_await;
                std::time_t full;
            };

        }  // namespace io
    }      // namespace quic
}  // namespace utils
