/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "comb.h"

namespace utils::minilang::comb {
    // constant Token
    struct CToken {
        const char* token;

        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            const size_t begin = seq.rptr;
            if (!seq.seek_if(token)) {
                return CombStatus::not_match;
            }
            const size_t end = seq.rptr;
            ctx.ctoken(token, Pos{begin, end});
            return CombStatus::match;
        }
    };

    constexpr CToken ctoken(const char* tok) {
        return CToken{tok};
    }

    // consume Token without callback
    struct CConsume {
        const char* token;

        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            if (!seq.seek_if(token)) {
                return CombStatus::not_match;
            }
            return CombStatus::match;
        }
    };

    // consume Token without callback
    constexpr CConsume cconsume(const char* tok) {
        return CConsume{tok};
    }

    template <class Fn>
    struct Peek : opti::MaybeEmptyA<Fn> {
        using opti::MaybeEmptyA<Fn>::MaybeEmptyA;

        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            const size_t ptr = seq.rptr;
            CombStatus ok = this->useA()(seq, ctx, r);
            seq.rptr = ptr;  // rollback
            return ok;
        }
    };

    // fn should not affect context
    template <class Fn>
    constexpr auto peekfn(Fn&& fn) {
        return Peek<std::decay_t<Fn>>{std::forward<Fn>(fn)};
    }

    // returns peekfn(cconsume(token))
    constexpr auto cpeek(const char* token) {
        return peekfn(cconsume(token));
    }

    // peek and should not match Token
    struct CNot {
        const char* token;
        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            if (seq.match(token)) {
                return CombStatus::not_match;
            }
            return CombStatus::match;
        }
    };

    template <class A>
    constexpr bool is_C_Token_v = std::is_same_v<A, CToken> ||
                                  std::is_same_v<A, CNot> ||
                                  std::is_same_v<A, CConsume>;

    // check token which should not match and then check token which should match
    // not_then(a,b,c) = CNot{a}&CNot{b}&CToken{c}
    constexpr auto not_then(auto tok) {
        if constexpr (is_C_Token_v<decltype(tok)>) {
            return CToken{tok.token};
        }
        else {
            return CToken{tok};
        }
    }

    // check token which should not match and then check token which should match
    // not_then(a,b,c) = CNot{a}&CNot{b}&CToken{c}
    constexpr auto not_then(auto a, auto b, auto... other) {
        if constexpr (is_C_Token_v<decltype(a)>) {
            return CNot{a.token} & not_then(b, other...);
        }
        else {
            return CNot{a} & not_then(b, other...);
        }
    }

    template <class Tag, class Fn>
    struct ConditionalRange : opti::MaybeEmptyA<Fn> {
        Tag tag;

        constexpr ConditionalRange(auto&& tag_, auto&& fn_)
            : tag(std::forward<decltype(tag_)>(tag_)),
              opti::MaybeEmptyA<Fn>(std::forward<decltype(fn_)>(fn_)) {}

        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            const auto ptr = seq.rptr;
            CombStatus s = this->useA()(seq, ctx, r);
            if (s == CombStatus::not_match) {
                seq.rptr = ptr;
                return CombStatus::not_match;
            }
            if (s == CombStatus::fatal || seq.rptr < ptr) {
                ctx.commit(CombMode::other, 0, CombState::fatal);
                return CombStatus::fatal;
            }
            const auto end = seq.rptr;
            ctx.crange(tag, Pos{ptr, end}, seq);
            seq.rptr = end;
            return CombStatus::match;
        }
    };

    // not in condition (foward reading)
    template <class Fn>
    struct NotConditionalRange : opti::MaybeEmptyA<Fn> {
        using opti::MaybeEmptyA<Fn>::MaybeEmptyA;

        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            const auto ptr = seq.rptr;
            CombStatus s = this->useA()(seq, ctx, r);
            seq.rptr = ptr;  // anyway rollback
            if (s == CombStatus::match) {
                return CombStatus::not_match;
            }
            if (s == CombStatus::fatal) {
                ctx.commit(CombMode::other, 0, CombState::fatal);
                return CombStatus::fatal;
            }
            return CombStatus::match;
        }
    };

    // Grouping
    template <class Tag, class A>
    struct Group : opti::MaybeEmptyA<A> {
        Tag tag;

        constexpr Group(auto&& tag_, auto&& fn_)
            : tag(std::forward<decltype(tag_)>(tag_)),
              opti::MaybeEmptyA<A>(std::forward<decltype(fn_)>(fn_)) {}

        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            std::uint64_t branch_id = 0;
            ctx.group(tag, &branch_id);
            const CombStatus res = this->useA()(seq, ctx, r);
            if (res == CombStatus::fatal) {
                return res;
            }
            ctx.group_end(tag, branch_id, res == CombStatus::match ? CombState::match : CombState::not_match);
            return res;
        }
    };

    // match to end of sequence
    struct EOS {
        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            if (seq.eos()) {
                return CombStatus::match;
            }
            return CombStatus::not_match;
        }
    };

    // match to begin of sequence
    struct BOS {
        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            if (seq.rptr == 0) {
                return CombStatus::match;
            }
            return CombStatus::not_match;
        }
    };

    // match to begin of line
    struct BOL {
        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            if (seq.rptr == 0 || seq.current(-1) == '\r' || seq.current(-1) == '\n') {
                return CombStatus::match;
            }
            return CombStatus::not_match;
        }
    };

    constexpr auto bol = BOL{};
    constexpr auto bos = BOS{};
    constexpr auto eos = EOS{};
    constexpr auto not_eos = not_(eos);

    template <class A>
    struct Proxy : opti::MaybeEmptyA<A> {
        using opti::MaybeEmptyA<A>::MaybeEmptyA;

        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            return this->useA()(seq, ctx, r);
        }
    };

    template <class Tag, class A>
    constexpr auto group(Tag&& tag, A&& a) {
        return Group<std::decay_t<Tag>, std::decay_t<A>>{
            std::forward<Tag>(tag),
            std::forward<A>(a),
        };
    }

    template <class Tag, class Fn>
    constexpr auto condfn(Tag&& tag, Fn&& fn) {
        return ConditionalRange<std::decay_t<Tag>, std::decay_t<Fn>>{
            std::forward<Tag>(tag),
            std::forward<Fn>(fn),
        };
    }

    // not consume but check fn() returns not_match
    template <class Fn>
    constexpr auto not_condfn(Fn&& fn) {
        return NotConditionalRange<std::decay_t<Fn>>{
            std::forward<Fn>(fn),
        };
    }

    template <class Fn>
    constexpr auto proxy(Fn&& fn) {
        return Proxy<std::decay_t<Fn>>{
            std::forward<Fn>(fn),
        };
    }

#define COMB_PROXY(name) proxy([](auto& seq, auto& ctx, auto&& rec) { return rec.name(seq, ctx, rec); })

    namespace test {
        constexpr bool check_token() {
            constexpr auto parse = group("group", CToken{"hey"} & -(CToken{" "} & +CToken{"jude"}));
            NullCtx nctx;
            auto obj = make_ref_seq("obj");
            if (parse(obj, nctx, 0) != CombStatus::not_match) {
                return false;
            }
            auto hey = make_ref_seq("hey");
            if (parse(hey, nctx, 0) != CombStatus::match) {
                return false;
            }
            auto hey_ = make_ref_seq("hey ");
            if (parse(hey_, nctx, 0) != CombStatus::fatal) {
                return false;
            }
            auto hey_jude = make_ref_seq("hey jude");
            if (parse(hey_jude, nctx, 0) != CombStatus::match) {
                return false;
            }
            return true;
        }
        static_assert(check_token());
    }  // namespace test

}  // namespace utils::minilang::comb
