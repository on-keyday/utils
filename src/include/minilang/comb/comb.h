/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <concepts>
#include <type_traits>
#include "nullctx.h"
#include "opti.h"

namespace utils::minilang::comb {

    namespace test {
        constexpr void error_if_constexpr(auto... args) {
            if (std::is_constant_evaluated()) {
                throw "error";
            }
        }

        template <class T, template <class...> class Templ>
        struct template_instance_of_t : std::false_type {};
        template <class U, template <class...> class Templ>
        struct template_instance_of_t<Templ<U>, Templ> : std::true_type {};

        template <class T, template <class...> class Templ>
        concept is_template_instance_of = template_instance_of_t<T, Templ>::value;

        template <class A>
        struct Test {};

        static_assert(is_template_instance_of<Test<int>, Test> && !is_template_instance_of<int, Test>);

        template <class A, CombStatus expect>
        struct StopOnTest : opti::MaybeEmptyA<A> {
            using opti::MaybeEmptyA<A>::MaybeEmptyA;
            template <class T, class Ctx, class Rec>
            constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
                const CombStatus res = this->useA()(seq, ctx, r);
                const size_t rptr = seq.rptr;
                if (res != expect) {
                    error_if_constexpr(rptr, res);
                }
                return res;
            };
        };

        template <CombStatus expect = CombStatus::match, class T>
        constexpr auto test_range(T&& t) {
            return StopOnTest<std::decay_t<T>, expect>{
                std::forward<T>(t),
            };
        }
    }  // namespace test

    template <class A, class B>
    struct And : opti::MaybeEmptyAB<A, B> {
        using opti::MaybeEmptyAB<A, B>::MaybeEmptyAB;
        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            const auto res = this->useA()(seq, ctx, r);
            if (res != CombStatus::match) {
                return res;
            }
            return this->useB()(seq, ctx, r);
        };
    };

    template <class A, class B>
    struct Or : opti::MaybeEmptyAB<A, B> {
        using opti::MaybeEmptyAB<A, B>::MaybeEmptyAB;
        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            std::uint64_t branch_id = 0;
            ctx.branch(CombMode::branch, &branch_id);
            const auto ptr = seq.rptr;
            const auto res = this->useA()(seq, ctx, r);
            if (res == CombStatus::fatal) {
                return res;
            }
            if (res == CombStatus::match) {
                ctx.commit(CombMode::branch, branch_id, CombState::match);
                return CombStatus::match;
            }
            ctx.commit(CombMode::branch, branch_id, CombState::other_branch);
            seq.rptr = ptr;
            const auto res2 = this->useB()(seq, ctx, r);
            if (res2 == CombStatus::fatal) {
                return res2;
            }
            if (res2 == CombStatus::match) {
                ctx.commit(CombMode::branch, branch_id, CombState::match);
                return CombStatus::match;
            }
            ctx.commit(CombMode::branch, branch_id, CombState::not_match);
            return CombStatus::not_match;
        }
    };

    template <class A>
    struct Optional : opti::MaybeEmptyA<A> {
        using opti::MaybeEmptyA<A>::MaybeEmptyA;

        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            std::uint64_t branch_id = 0;
            ctx.branch(CombMode::optional, &branch_id);
            const CombStatus res = this->useA()(seq, ctx, r);
            if (res == CombStatus::fatal) {
                return res;
            }
            ctx.commit(CombMode::optional, branch_id, res == CombStatus::match ? CombState::match : CombState::ignore_not_match);
            return CombStatus::match;
        }
    };

    template <class A>
    struct Repeat : opti::MaybeEmptyA<A> {
        static_assert(!test::is_template_instance_of<A, Optional>, "`~-` is not allowed. use `-~` instead");
        using opti::MaybeEmptyA<A>::MaybeEmptyA;

        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            std::uint64_t branch_id = 0;
            ctx.branch(CombMode::repeat, &branch_id);
            bool first = true;
            for (;;) {
                const auto ptr = seq.rptr;
                const CombStatus res = this->useA()(seq, ctx, r);
                if (res == CombStatus::fatal) {
                    return res;
                }
                if (res == CombStatus::not_match) {
                    seq.rptr = ptr;
                    break;
                }
                // detect infinity loop model
                if (seq.rptr <= ptr) {
                    ctx.commit(CombMode::repeat, branch_id, CombState::fatal);
                    return CombStatus::fatal;
                }
                ctx.commit(CombMode::repeat, branch_id, CombState::match);
                first = false;
            }
            ctx.commit(CombMode::repeat, branch_id, first ? CombState::not_match : CombState::repeat_match);
            return first ? CombStatus::not_match : CombStatus::match;
        }
    };

    template <class A>
    struct MustMatch : opti::MaybeEmptyA<A> {
        using opti::MaybeEmptyA<A>::MaybeEmptyA;
        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            std::uint64_t branch_id = 0;
            ctx.branch(CombMode::must_match, &branch_id);
            const CombStatus res = this->useA()(seq, ctx, r);
            if (res == CombStatus::fatal) {
                return res;
            }
            if (res == CombStatus::not_match) {
                ctx.commit(CombMode::must_match, branch_id, CombState::fatal);
                return CombStatus::fatal;
            }
            ctx.commit(CombMode::must_match, branch_id, CombState::match);
            return CombStatus::match;
        }
    };

    template <class A>
    struct Not : opti::MaybeEmptyA<A> {
        using opti::MaybeEmptyA<A>::MaybeEmptyA;
        template <class T, class Ctx, class Rec>
        constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
            const CombStatus res = this->useA()(seq, ctx, r);
            if (res == CombStatus::fatal) {
                return res;
            }
            return res == CombStatus::not_match ? CombStatus::match : CombStatus::not_match;
        }
    };

    template <class A, class B>
    constexpr auto operator&(A&& a, B&& b) {
        return And<std::decay_t<A>, std::decay_t<B>>{
            std::forward<A>(a),
            std::forward<B>(b),
        };
    }

    template <class A, class B>
    constexpr auto operator|(A&& a, B&& b) {
        return Or<std::decay_t<A>, std::decay_t<B>>{
            std::forward<A>(a),
            std::forward<B>(b),
        };
    }
    // optional (weaker)
    template <class A>
    constexpr auto operator-(A&& a) {
        return Optional<std::decay_t<A>>{
            std::forward<A>(a),
        };
    }

    // repeat
    template <class A>
    constexpr auto operator~(A&& a) {
        return Repeat<std::decay_t<A>>{
            std::forward<A>(a),
        };
    }

    // must match (stronger)
    template <class A>
    constexpr auto operator+(A&& a) {
        return MustMatch<std::decay_t<A>>{
            std::forward<A>(a),
        };
    }

    template <class A>
    constexpr auto not_(A&& a) {
        return Not<std::decay_t<A>>{
            std::forward<A>(a),
        };
    }

    namespace test {
        struct Null {
            template <class T, class Ctx, class Rec>
            constexpr CombStatus operator()(Sequencer<T>& seq, Ctx& ctx, Rec&&) const {
                return CombStatus::match;
            }
        };
        constexpr void check_operator() {
            constexpr auto null_parse = +Null{} & -Null{} | ~Null{};
        }

    }  // namespace test
}  // namespace utils::minilang::comb
