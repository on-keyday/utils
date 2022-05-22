/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// stream - parser stream
#pragma once

#include "../../iface/error_log.h"
#include "../../helper/appender.h"

namespace utils {
    namespace parser {
        namespace stream {
            struct Error : iface::Error<iface::Powns, Error> {
                using iface::Error<iface::Powns, Error>::Error;
            };

            namespace internal {
                template <class T>
                concept has_child = requires(T t) {
                    {t.child(size_t{})};
                };
            }  // namespace internal

            struct TokenInfo {
                const char* kind;
                size_t pos;
                size_t len;
                size_t child;
                bool fixed_child;
                bool has_err;
            };

            struct TokenBase : iface::Powns {
               private:
                MAKE_FN_VOID(token, iface::PushBacker<iface::Ref>)
                MAKE_FN(info, TokenInfo)
                MAKE_FN(err, Error)

                size_t (*child_ptr)(void*, size_t) = nullptr;

                template <class T>
                static Stream child_fn(void* ptr, size_t i) {
                    if constexpr (internal::has_child<T>) {
                        return static_cast<T*>(ptr)->child(i);
                    }
                    else {
                        return Stream{};
                    }
                }

               public:
                DEFAULT_METHODS(TokenBase)
                template <class T>
                TokenBase(T&& t)
                    : APPLY2_FN(token, iface::PushBacker<iface::Ref>),
                      APPLY2_FN(info),
                      APPLY2_FN(err),
                      APPLY2_FN(child),
                      iface::Powns(std::forward<T>(t)) {}

                void token(iface::PushBacker<iface::Ref> out) const {
                    DEFAULT_CALL(token, (void)0, out);
                }

                TokenInfo info() const {
                    DEFAULT_CALL(info, TokenInfo{0});
                }

                Error err() {
                    DEFAULT_CALL(err, Error{});
                }

                Token child(size_t i) const {
                    DEFAULT_CALL(child, Token{}, i)
                }
            };

            using Token = iface::Copy<TokenBase>;
            using PB = iface::PushBacker<iface::Ref>;

            constexpr bool has_err(const Token& token) {
                return token.info().has_err;
            }

            struct simpleErrToken {
                const char* msg;
                constexpr void token(PB pb) const {
                    helper::append(pb, msg);
                }

                constexpr TokenInfo info() const {
                    auto tok = TokenInfo{0};
                    tok.kind = "simpleErrToken";
                    tok.has_err = true;
                    return tok;
                }

                Error err() const {
                    return *this;
                }

                constexpr void error(PB pb) const {
                    helper::append(pb, msg);
                }
            };

            struct StreamInfo {
                const char* kind;
                bool broken;
                bool eos;
                size_t pos;
                size_t exists_token;
                size_t raw_bytes_remain;
                size_t child_stream;
            };

            constexpr auto ErrNullStream = simpleErrToken{"null stream"};

            struct Stream : iface::Powns {
               private:
                MAKE_FN(get_token, Token)
                MAKE_FN_VOID(push_token, Token&&)
                MAKE_FN(info, StreamInfo)
                size_t (*child_ptr)(void*, size_t) = nullptr;

                template <class T>
                static Stream child_fn(void* ptr, size_t i) {
                    if constexpr (internal::has_child<T>) {
                        return static_cast<T*>(ptr)->child(i);
                    }
                    else {
                        return Stream{};
                    }
                }

                static StreamInfo null_info() {
                    StreamInfo info{0};
                    info.kind = "NULL";
                    info.broken = true;
                    info.eos = true;
                    return info;
                }

               public:
                DEFAULT_METHODS(Stream)

                template <class T>
                Stream(T&& t)
                    : APPLY2_FN(get_token),
                      APPLY2_FN(push_token, Token &&),
                      APPLY2_FN(info),
                      APPLY2_FN(child),
                      iface::Powns(std::forward<T>(t)) {}

                Token get_token() {
                    DEFAULT_CALL(get_token, ErrNullStream);
                }

                void push_token(Token&& token) {
                    DEFAULT_CALL(push_token, (void)0, std::move(token));
                }

                StreamInfo info() const {
                    DEFAULT_CALL(info, null_info());
                }

                Stream child(size_t i) {
                    DEFAULT_CALL(child, Stream{}, i);
                }
            };
        }  // namespace stream
    }      // namespace parser
}  // namespace utils
