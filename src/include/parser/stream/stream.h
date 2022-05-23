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
#include "../../iface/macros.h"

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

                template <class T>
                concept has_info = requires(T t) {
                    {t.info()};
                };

                template <class T>
                concept has_raw = requires(T t) {
                    {t.raw()};
                };

                template <class T, class Stream>
                concept has_substream = requires(T t) {
                    {t.substream(Stream{})};
                };

                template <class T>
                concept has_truncate = requires(T t) {
                    {t.truncate()};
                };

            }  // namespace internal

            struct TokenInfo {
                const char* kind;
                size_t pos;
                size_t len;
                size_t child;
                size_t end_child_pos;
                bool fixed_child;
                bool has_err;
            };

            struct TokenBase : iface::Powns {
               private:
                using Token = iface::Copy<TokenBase>;
                MAKE_FN_VOID(token, iface::PushBacker<iface::Ref>)
                MAKE_FN_EXISTS(info, TokenInfo, internal::has_info<T>, {})
                MAKE_FN(err, Error)
                MAKE_FN_EXISTS(child, Token, internal::has_child<T>, {}, size_t)

               public:
                DEFAULT_METHODS_MOVE(TokenBase)
                template <class T>
                TokenBase(T&& t)
                    : APPLY2_FN(token, iface::PushBacker<iface::Ref>),
                      APPLY2_FN(info),
                      APPLY2_FN(err),
                      APPLY2_FN(child, size_t),
                      iface::Powns(std::forward<T>(t)) {}

                void token(iface::PushBacker<iface::Ref> out) const {
                    DEFAULT_CALL(token, (void)0, out);
                }

                template <class Str = iface::fixedBuf<100>>
                Str stoken() const {
                    Str tok;
                    token(tok);
                    return tok;
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

            inline bool has_err(const Token& token) {
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

            struct InputStat {
                bool has_info;
                size_t pos;
                size_t remain;
                bool raw;
                bool eos;
                bool err;
            };

            struct Input : iface::Ref {
               private:
                MAKE_FN(peek, size_t, char*, size_t)
                MAKE_FN(seek, bool, std::size_t)
                MAKE_FN_EXISTS(raw, Input, internal::has_raw<T>, {})
                MAKE_FN_EXISTS(info, InputStat, internal::has_info<T>, {})
                MAKE_FN_EXISTS(truncate, bool, internal::has_truncate<T>, false)

                static InputStat err_stat() {
                    InputStat stat{0};
                    stat.eos = true;
                    stat.err = true;
                    return stat;
                }

                bool input_rotuine(const InputStat& info_, const char* p, size_t size,
                                   char* buf, size_t len, bool consume) {
                    size_t count = 0;
                    while (true) {
                        const size_t remain = size - count;
                        const size_t to_fetch = remain < len ? remain : len;
                        const size_t fetched = peek(buf, to_fetch);
                        if (fetched < to_fetch) {
                            seek(info_.pos);
                            return false;
                        }
                        if (::strncmp(p + count, buf, fetched) != 0) {
                            seek(info_.pos);
                            return false;
                        }
                        count += fetched;
                        if (count >= size) {
                            break;
                        }
                        if (!seek(info_.pos + count)) {
                            return false;
                        }
                    }
                    if (consume) {
                        return seek(info_.pos + size);
                    }
                    seek(info_.pos);
                    return true;
                }

               public:
                DEFAULT_METHODS(Input)
                template <class T>
                Input(T&& t)
                    : APPLY2_FN(peek, char*, size_t),
                      APPLY2_FN(seek, std::size_t),
                      APPLY2_FN(info),
                      APPLY2_FN(raw),
                      APPLY2_FN(truncate),
                      iface::Ref(t) {}

                size_t peek(char* ptr, size_t size) {
                    DEFAULT_CALL(peek, 0, ptr, size);
                }

                bool consume(const char* p, size_t size = 0, char* tmpbuf = nullptr, size_t bufsize = 0) {
                    return expect(p, size, tmpbuf, bufsize, true);
                }

                bool expect(const char* p, size_t size = 0, char* tmpbuf = nullptr, size_t bufsize = 0, bool consume = false) {
                    if (!p) {
                        return false;
                    }
                    if (size == 0) {
                        size = ::strlen(p);
                    }
                    const auto info_ = info();
                    if (info_.has_info) {
                        if (info_.remain < size) {
                            return false;
                        }
                    }
                    if (tmpbuf && bufsize) {
                        return input_rotuine(info_, p, size, tmpbuf, bufsize, consume);
                    }
                    else if (size >= 100) {
                        char buf[100];
                        return input_rotuine(info_, p, size, buf, 100, consume);
                    }
                    else {
                        char buf[20];
                        return input_rotuine(info_, p, size, buf, 20, consume);
                    }
                }

                bool seek(size_t pos) {
                    DEFAULT_CALL(seek, false, pos);
                }

                bool offset_seek(std::int64_t pos) {
                    auto info_ = info();
                    return seek(info_.pos + pos);
                }

                InputStat info() {
                    DEFAULT_CALL(info, err_stat());
                }

                Input raw() {
                    DEFAULT_CALL(raw, Input{});
                }

                bool truncate() {
                    DEFAULT_CALL(truncate, false);
                }
            };

            struct StreamInfo {
                size_t limsub;
                size_t cursub;
            };

            struct Stream : iface::Powns {
               private:
                template <class T>
                static constexpr bool has_substream = internal::has_substream<T, Stream>;
                MAKE_FN_EXISTS(substream, bool, has_substream<T>, false, Stream&&)
                MAKE_FN_EXISTS(info, StreamInfo, internal::has_info<T>, {0})
                MAKE_FN(parse, Token, Input&)

                static Token err_token() {
                    return simpleErrToken{"NULL Stream"};
                }

               public:
                DEFAULT_METHODS_MOVE(Stream)
                template <class T>
                Stream(T&& t)
                    : APPLY2_FN(substream, Stream &&),
                      APPLY2_FN(info),
                      APPLY2_FN(parse, Input&),
                      iface::Powns(std::forward<T>(t)) {}

                bool substream(Stream st) {
                    DEFAULT_CALL(substream, false, std::move(st));
                }

                StreamInfo info(){
                    DEFAULT_CALL(info, StreamInfo{})}

                Token parse(Input input) {
                    DEFAULT_CALL(parse, err_token(), input)
                }
            };
        }  // namespace stream
    }      // namespace parser
}  // namespace utils

#include "../../iface/undef_macros.h"
