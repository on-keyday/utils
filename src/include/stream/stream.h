/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// stream - parser stream
#pragma once

#include <iface/error_log.h>
#include <helper/appender.h>
#include <helper/equal.h>
#include <iface/macros.h>
#include <wrap/light/enum.h>

namespace utils {
    namespace parser {
        namespace stream {

            enum class EKind {
                fatal,
                recover,
                retry,
                wrapped,
            };

            BEGIN_ENUM_STRING_MSG(EKind, k)
            ENUM_STRING_MSG(EKind::fatal, "fatal")
            ENUM_STRING_MSG(EKind::recover, "recover")
            ENUM_STRING_MSG(EKind::retry, "retry")
            ENUM_STRING_MSG(EKind::wrapped, "wrapped")
            END_ENUM_STRING_MSG("")

            struct Error : iface::Error<iface::Powns, Error> {
                using iface::Error<iface::Powns, Error>::Error;
                bool is(EKind kind) {
                    return kind_of(k(kind));
                }
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
                    {t.substream(Stream{}, "key")};
                };

                template <class T>
                concept has_truncate = requires(T t) {
                    {t.truncate()};
                };

            }  // namespace internal

            struct TokenInfo {
                const char* kind;
                const char* dirtok;
                size_t pos;
                size_t len;
                size_t child;
                size_t end_child_pos;
                bool fixed_child;
                bool has_err;
            };

            namespace internal {
                template <class T>
                concept has_err = requires(T t) {
                    {t.err()};
                };
            }  // namespace internal

            struct TokenBase : iface::Powns {
               private:
                using Token = iface::Copy<TokenBase>;
                MAKE_FN_VOID(token, iface::PushBacker<iface::Ref>)
                MAKE_FN_EXISTS(info, TokenInfo, internal::has_info<T>, {})
                MAKE_FN_EXISTS(err, Error, internal::has_err<T>, {})
                MAKE_FN_EXISTS(child, Token, internal::has_child<T>, {}, size_t)

               public:
                DEFAULT_METHODS_MOVE(TokenBase)
                template <class T,
                          REJECT_SELF(T, TokenBase)>
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
                    DEFAULT_CALL(child, Token{}, i);
                }

                size_t pos() const {
                    return info().pos;
                }
            };

            using Token = iface::Copy<TokenBase>;
            using PB = iface::PushBacker<iface::Ref>;

            inline bool has_err(const Token& token) {
                return token.info().has_err;
            }

            inline bool is_kind(const Token& token, const char* key) {
                return helper::equal(token.info().kind, key);
            }

            template <class T>
            T* as(Token& tok) {
                return static_cast<T*>(tok.ptr());
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

            struct StreamBase;
            using Stream = iface::Copy<StreamBase>;

            template <class T>
            concept has_change = requires(T t) {
                {t.change("context")};
            };

            template <class T>
            concept has_add = requires(T t) {
                {t.add("record", std::declval<const Token&>())};
            };

            template <class T>
            concept has_scope = requires(T t, bool enter) {
                {t.scope("record", enter)};
            };

            struct SemanticsBase : iface::Powns {
               private:
                using Semantics = iface::Copy<SemanticsBase>;
                MAKE_FN(on, bool, const char*)
                MAKE_FN_EXISTS(change, const char*, has_change<T>, nullptr, const char*)
                MAKE_FN_EXISTS(add, Error, has_add<T>, {}, const char*, const Token&)
                MAKE_FN_EXISTS(scope, Error, has_scope<T>, {}, const char*, bool)
               public:
                DEFAULT_METHODS_MOVE(SemanticsBase)
                template <class T, REJECT_SELF(T, SemanticsBase)>
                SemanticsBase(T&& t)
                    : APPLY2_FN(on, const char*),
                      APPLY2_FN(add, const char*, const Token&),
                      APPLY2_FN(change, const char*),
                      APPLY2_FN(scope, const char*, bool),
                      iface::Powns(std::forward<T>(t)) {}

                bool on(const char* name) const {
                    DEFAULT_CALL(on, false, name);
                }

                const char* change(const char* name) {
                    DEFAULT_CALL(change, nullptr, name);
                }

                Error add(const char* record, const Token& tok) {
                    DEFAULT_CALL(add, Error{}, record, tok);
                }

                Error scope(const char* record, bool enter) {
                    DEFAULT_CALL(scope, Error{}, record, enter);
                }
            };

            using Semantics = iface::Copy<SemanticsBase>;

            struct InputStat {
                size_t pos;
                size_t remain;
                bool sized;
                bool raw;
                bool eos;
                bool err;
                Semantics* semantic_context;
                Stream* trimming_stream;
            };

            template <class T>
            concept consumer_condition_1 = requires(T t) {
                {t("string", 7, std::declval<size_t*>())};
            };

            template <class T>
            concept consumer_condition_2 = requires(T t) {
                {t("string", 7)};
            };

            template <class T>
            concept consumer_condition_3 = requires(T t) {
                {t("string")};
            };

            constexpr auto consumeStop = false;
            constexpr auto consumeContinue = true;

            struct Input : iface::Ref {
               private:
                MAKE_FN(peek, const char*, char*, size_t, size_t*)
                MAKE_FN(seek, bool, std::size_t)
                MAKE_FN(info, InputStat)
                MAKE_FN_EXISTS(raw, Input, internal::has_raw<T>, {})
                MAKE_FN_EXISTS(truncate, bool, internal::has_truncate<T>, false)

                static InputStat err_stat() {
                    InputStat stat{0};
                    stat.eos = true;
                    stat.err = true;
                    return stat;
                }

                template <class Callback>
                bool input_rotuine(const InputStat& info_, size_t size,
                                   char* buf, size_t len, bool consume, bool besteffort,
                                   Callback cb) {
                    size_t count = 0;
                    while (true) {
                        const size_t remain = size - count;
                        const size_t to_fetch = remain < len ? remain : len;
                        size_t red = 0;
                        const char* fetched = peek(buf, to_fetch, &red);
                        if (!fetched || (!besteffort && red < to_fetch)) {
                            seek(info_.pos);
                            return false;
                        }
                        if (!cb(info_.pos, count, fetched, red, to_fetch)) {
                            seek(info_.pos);
                            return false;
                        }
                        count += red;
                        if (count >= size || red < to_fetch) {
                            break;
                        }
                        if (!seek(info_.pos + count)) {
                            return false;
                        }
                    }
                    if (consume) {
                        auto offset = count <= size ? count : size;
                        return seek(info_.pos + offset);
                    }
                    seek(info_.pos);
                    return true;
                }

               public:
                DEFAULT_METHODS(Input)
                template <class T>
                Input(T&& t)
                    : APPLY2_FN(peek, char*, size_t, size_t*),
                      APPLY2_FN(seek, std::size_t),
                      APPLY2_FN(info),
                      APPLY2_FN(raw),
                      APPLY2_FN(truncate),
                      iface::Ref(t) {}

                const char* peek(char* ptr, size_t size, size_t* red) {
                    DEFAULT_CALL(peek, 0, ptr, size, red);
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
                    if (info_.sized && info_.remain < size) {
                        return false;
                    }
                    auto cb = [&](size_t pos, size_t count, const char* buf, size_t fetched, size_t to_fetch) {
                        return ::strncmp(p + count, buf, to_fetch) == 0;
                    };
                    if (tmpbuf && bufsize) {
                        return input_rotuine(info_, size, tmpbuf, bufsize, consume, false, cb);
                    }
                    else if (size >= 100) {
                        char buf[100];
                        return input_rotuine(info_, size, buf, 100, consume, false, cb);
                    }
                    else {
                        char buf[20];
                        return input_rotuine(info_, size, buf, 20, consume, false, cb);
                    }
                }

                template <class Cond>
                size_t consume_if(Cond&& cond) {
                    auto info_ = info();
                    if (info_.eos) {
                        return 0;
                    }
                    char buf[50];
                    bool ended = false;
                    size_t endpos = 0;
                    size_t ret = 0;
                    bool size_limited = false;
                    auto cb = [&](size_t pos, size_t count, const char* buf, size_t fetched, size_t to_fetch) {
                        size_limited = false;
                        for (size_t i = 0; i < fetched; i++) {
                            bool check = false;
                            if constexpr (consumer_condition_1<Cond>) {
                                check = cond(buf + i, fetched - i, &i);
                            }
                            else if constexpr (consumer_condition_2<Cond>) {
                                check = cond(buf + i, fetched);
                            }
                            else {
                                static_assert(consumer_condition_3<Cond>, "require condition");
                                check = cond(buf + i);
                            }
                            if (!check) {
                                endpos = pos + count + i;
                                ret = count + i;
                                ended = true;
                                return false;
                            }
                        }
                        size_limited = true;
                        endpos = pos + count + fetched;
                        ret = count + fetched;
                        return true;
                    };
                    while (true) {
                        auto res = input_rotuine(info_, 100, buf, 50, false, true, cb);
                        if (ended || size_limited) {
                            break;
                        }
                    }
                    seek(endpos);
                    return ret;
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

                size_t pos() {
                    return info().pos;
                }

                bool truncate() {
                    DEFAULT_CALL(truncate, false);
                }
            };

            inline Semantics* get_semantics(Input& input) {
                return input.info().semantic_context;
            }

            inline Stream* get_trimminger(Input& input) {
                return input.info().trimming_stream;
            }

            struct StreamInfo {
                size_t limsub;
                size_t cursub;
            };

            struct StreamBase : iface::Powns {
               private:
                using Stream = iface::Copy<StreamBase>;
                template <class T>
                static constexpr bool has_substream = internal::has_substream<T, Stream>;
                MAKE_FN_EXISTS(substream, bool, has_substream<T>, false, Stream&&, const char*)
                MAKE_FN_EXISTS(info, StreamInfo, internal::has_info<T>, {0})
                MAKE_FN(parse, Token, Input&)

                static Token err_token() {
                    return simpleErrToken{"NULL Stream"};
                }

               public:
                DEFAULT_METHODS_MOVE(StreamBase)
                template <class T, REJECT_SELF(T, StreamBase)>
                StreamBase(T&& t)
                    : APPLY2_FN(substream, Stream &&),
                      APPLY2_FN(info),
                      APPLY2_FN(parse, Input&),
                      iface::Powns(std::forward<T>(t)) {}

                bool substream(Stream st, const char* key) {
                    DEFAULT_CALL(substream, false, std::move(st), key);
                }

                StreamInfo info() {
                    DEFAULT_CALL(info, StreamInfo{});
                }

                Token parse(Input&& input) {
                    DEFAULT_CALL(parse, err_token(), input);
                }

                Token parse(Input& input) {
                    DEFAULT_CALL(parse, err_token(), input);
                }
            };

            using Stream = iface::Copy<StreamBase>;
        }  // namespace stream
    }      // namespace parser
}  // namespace utils

#include <iface/undef_macros.h>
