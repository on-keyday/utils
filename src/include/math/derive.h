/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cmath>
#include "../binary/float.h"
#include <tuple>
#include "../number/to_string.h"
#include "../strutil/append.h"

namespace utils::math {

    struct FromFloat {
        std::uint64_t value = 0;

        constexpr FromFloat(double d) {
            value = binary::DoubleFloat{d}.to_int();
        }

        constexpr FromFloat(std::int64_t v)
            : FromFloat(double(v)) {
        }

        constexpr FromFloat(int v)
            : FromFloat(std::int64_t(v)) {
        }

        constexpr auto to_float() const {
            return binary::DoubleFloat{value}.to_float();
        }

        friend constexpr FromFloat operator+(FromFloat a, FromFloat b) {
            return a.to_float() + b.to_float();
        }

        friend constexpr FromFloat operator*(FromFloat a, FromFloat b) {
            return a.to_float() * b.to_float();
        }

        friend constexpr FromFloat operator/(FromFloat a, FromFloat b) {
            return a.to_float() / b.to_float();
        }
    };

    template <FromFloat a>
    struct Const {
        constexpr auto operator()(auto&&... x) const {
            return a.to_float();
        }

        constexpr auto raw() const {
            return a;
        }

        constexpr auto derive() const {
            return Const<0>{};
        }

        constexpr auto print(auto&& out) const {
            auto f = a.to_float();
            number::to_string(out, f);
        }
    };

    struct X {
        constexpr auto operator()(auto&& x) const {
            return x;
        }

        constexpr auto derive() const {
            return Const<1>{};
        }

        constexpr auto print(auto&& out) const {
            strutil::append(out, "x");
        }
    };

    constexpr auto pi = Const<3.14159265358979323846264338327950288>{};
    constexpr auto e = Const<2.71828182845904523536028747135266249>{};

    template <class T>
    struct derivable_t : std::false_type {};

    template <FromFloat n>
    struct derivable_t<Const<n>> : std::true_type {};

    template <class T>
    concept derivable0 = requires(T t) {
        { t.derive() };
    };

    template <class T>
        requires derivable0<T>
    struct derivable_t<T> : std::true_type {};

    template <class T>
    concept derivable = derivable_t<T>::value;

    template <class Apply, template <class...> class O, class A, class B>
    struct OpHelper {
        static constexpr auto is_const = false;
        using applied = O<A, B>;
    };

    template <class Apply, template <class...> class O, FromFloat a, FromFloat b>
    struct OpHelper<Apply, O, Const<a>, Const<b>> {
        static constexpr auto is_const = true;
        using applied = Const<Apply{}(a, b)>;
    };

    template <class T>
    struct is_Const_t : std::false_type {};
    template <FromFloat n>
    struct is_Const_t<Const<n>> : std::true_type {};

    template <class T>
    concept is_Const = is_Const_t<T>::value;

    struct ApplyAdd {
        constexpr auto operator()(auto a, auto b) {
            return a + b;
        }
    };

    struct ApplyMul {
        constexpr auto operator()(auto a, auto b) {
            return a * b;
        }
    };

    struct ApplyDiv {
        constexpr auto operator()(auto a, auto b) {
            return a / b;
        }
    };

    template <class A, class B>
    struct Add;

    template <class A, class B>
    struct Mul;

    template <class A, class B>
    struct Div;

    template <class A, class B>
    struct Pow;

    namespace internal {

        template <class T, class U>
        constexpr auto mul(T a, U b);

        template <class T, class U>
        constexpr auto add(T a, U b) {
            using Apply = OpHelper<ApplyAdd, Add, T, U>;
            using Result = typename Apply::applied;
            if constexpr (Apply::is_const) {
                return Result{};
            }
            else if constexpr (std::is_same_v<T, Const<0>>) {
                return b;
            }
            else if constexpr (std::is_same_v<U, Const<0>>) {
                return a;
            }
            else if constexpr (std::is_same_v<T, U>) {
                return mul(Const<2>{}, a);
            }
            else {
                return Result{};
            }
        }

        template <class T>
        struct MaybeConstMul : std::false_type {
        };

        template <FromFloat n, class U>
        struct MaybeConstMul<Mul<Const<n>, U>> : std::true_type {
            static constexpr auto a_is_const = true;
        };

        template <FromFloat n, class U>
        struct MaybeConstMul<Mul<U, Const<n>>> : std::true_type {
            static constexpr auto a_is_const = false;
        };

        template <class T, class U>
        constexpr auto mul(T a, U b) {
            if constexpr (std::is_same_v<T, U>) {
                return Pow<T, Const<2>>{};
            }
            else {
                using Apply = OpHelper<ApplyMul, Mul, T, U>;
                using Result = typename Apply::applied;
                if constexpr (Apply::is_const) {
                    return Result{};
                }
                else if constexpr (std::is_same_v<T, Const<1>>) {
                    return b;
                }
                else if constexpr (std::is_same_v<U, Const<1>>) {
                    return a;
                }
                else if constexpr (std::is_same_v<T, Const<0>> || std::is_same_v<U, Const<0>>) {
                    return Const<0>{};
                }
                else if constexpr (is_Const<T> && MaybeConstMul<U>::value) {
                    if constexpr (MaybeConstMul<U>::a_is_const) {
                        return mul(mul(a, b.a), b.b);
                    }
                    else {
                        return mul(mul(a, b.b), b.a);
                    }
                }
                else {
                    return Result{};
                }
            }
        }

        template <class T, class U>
        constexpr auto sub(T a, U b) {
            return internal::add(a, internal::mul(Const<-1>{}, b));
        }

        template <class T, class U>
        constexpr auto div(T a, U b) {
            using Apply = OpHelper<ApplyDiv, Div, T, U>;
            using Result = typename Apply::applied;
            if constexpr (Apply::is_const) {
                return Result{};
            }
            else if constexpr (std::is_same_v<U, Const<1>>) {
                return a;
            }
            else {
                return Result{};
            }
        }

        template <class T>
        struct Template : std::false_type {};

        template <class A, class B, template <class, class> class T>
        struct Template<T<A, B>> : std::true_type {
            static constexpr auto count = 2;

            template <class V, class U>
            using instance = T<V, U>;
        };

        template <class A, template <class> class T>
        struct Template<T<A>> : std::true_type {
            static constexpr auto count = 1;

            template <class V>
            using instance = T<V>;
        };

        template <class A, double (*f1)(double), double (*f2)(double), int phase, template <class, double (*)(double), double (*)(double), int> class T>
        struct Template<T<A, f1, f2, phase>> : std::true_type {
            static constexpr auto count = 1;
            template <class V>
            using instance = T<V, f1, f2, phase>;
        };

        constexpr auto replace_x(auto base, auto new_x) {
            using T = Template<decltype(base)>;
            if constexpr (is_Const<decltype(base)>) {
                return base;
            }
            else if constexpr (std::is_same_v<decltype(base), X>) {
                return new_x;
            }
            else if constexpr (T::value) {
                if constexpr (T::count == 1) {
                    using V = typename T::template instance<decltype(replace_x(base.a, new_x))>;
                    return V{};
                }
                else if constexpr (T::count == 2) {
                    using V = typename T::template instance<decltype(replace_x(base.a, new_x)), decltype(replace_x(base.b, new_x))>;
                    return V{};
                }
            }
            else {
                static_assert(std::is_same_v<decltype(base), X>);
            }
        }
    }  // namespace internal

    template <derivable T, derivable U>
    constexpr auto replace_x(T base, U new_x) {
        return internal::replace_x(base, new_x);
    }

    template <derivable T, derivable U>
    constexpr auto operator+(T a, U b) {
        return internal::add(a, b);
    }

    template <derivable T, derivable U>
    constexpr auto operator-(T a, U b) {
        return internal::sub(a, b);
    }

    template <derivable T, derivable U>
    constexpr auto operator*(T a, U b) {
        return internal::mul(a, b);
    }

    template <derivable T, derivable U>
    constexpr auto operator/(T a, U b) {
        return internal::div(a, b);
    }

    template <class A, class B>
    struct Add {
        static constexpr A a;
        static constexpr B b;

        constexpr auto operator()(auto&&... x) const {
            return a(x...) + b(x...);
        }

        constexpr auto derive() const {
            return internal::add(a.derive(), b.derive());
        }

        constexpr auto print(auto&& out) const {
            strutil::append(out, "(");
            a.print(out);
            strutil::append(out, " + ");
            b.print(out);
            strutil::append(out, ")");
        }
    };

    template <class A, class B>
    struct Mul {
        static constexpr A a;
        static constexpr B b;

        constexpr auto operator()(auto&&... x) const {
            return a(x...) * b(x...);
        }

        constexpr auto derive() const {
            return internal::add(internal::mul(a.derive(), b), internal::mul(a, b.derive()));
        }

        constexpr auto print(auto&& out) const {
            strutil::append(out, "(");
            a.print(out);
            strutil::append(out, " * ");
            b.print(out);
            strutil::append(out, ")");
        }
    };

    template <class A, class B>
    struct Div {
        static constexpr A a;
        static constexpr B b;

        constexpr auto operator()(auto&&... x) const {
            return a(x...) / b(x...);
        }

        constexpr auto derive() const {
            return internal::div(
                internal::sub(internal::mul(a.derive(), b), internal::mul(a, b.derive())),
                internal::mul(b, b));
        }

        constexpr auto print(auto&& out) const {
            strutil::append(out, "(");
            a.print(out);
            strutil::append(out, " / ");
            b.print(out);
            strutil::append(out, ")");
        }
    };

    template <FromFloat x>
    constexpr auto c() {
        return Const<x>{};
    }

    constexpr auto x = X{};

    template <class A, class B>
    struct Log {
        static_assert(is_Const<A>);
        static constexpr A a;
        static constexpr B b;

        constexpr auto operator()(auto&&... x) const {
            if constexpr (a.raw() == e.raw()) {
                return std::log(b(x...));
            }
            else {
                return std::log(b(x...)) / std::log(a.raw());
            }
        }

        constexpr auto derive() const {
            if constexpr (std::is_same_v<decltype(a), decltype(e)>) {
                // log(e) = 1
                return internal::div(Const<1>{}, b) * b.derive();
            }
            else {
                return internal::div(Const<1>{}, internal::mul(b, Log{e, a})) * b.derive();
            }
        }

        constexpr auto print(auto&& out) const {
            strutil::append(out, "log(");
            a.print(out);
            strutil::append(out, " , ");
            b.print(out);
            strutil::append(out, ")");
        }
    };

    template <class A, class B>
    struct Pow {
        static constexpr A a;
        static constexpr B b;

        constexpr auto operator()(auto&&... x) const {
            if constexpr (is_Const<A>) {
                if (a.raw() == e.raw()) {
                    return std::exp(b(x...));
                }
            }
            return std::pow(a(x...), b(x...));
        }

        constexpr auto derive() const {
            if constexpr (is_Const<B>) {
                auto n_ = internal::sub(b, Const<1>{});
                if constexpr (std::is_same_v<decltype(n_), Const<0>>) {
                    return internal::mul(b, a.derive());
                }
                else if constexpr (std::is_same_v<decltype(n_), Const<1>>) {
                    return internal::mul(internal::mul(b, a), a.derive());
                }
                else {
                    return internal::mul(internal::mul(b, Pow<A, decltype(n_)>{}), a.derive());
                }
            }
            else if constexpr (is_Const<A>) {
                if constexpr (std::is_same_v<A, decltype(e)>) {
                    return internal::mul(Pow{a, b}, b.derive());
                }
                else {
                    return internal::mul(internal::mul(Pow{a, b}, Log{e, a}), b.derive());
                }
            }
            else {
                static_assert(is_Const<A> || is_Const<B>);
            }
        }

        constexpr auto print(auto&& out) const {
            strutil::append(out, "(");
            a.print(out);
            strutil::append(out, " ^ ");
            b.print(out);
            strutil::append(out, ")");
        }
    };

    template <class A, class B>
    constexpr auto pow(A a, B b) {
        if constexpr (std::is_same_v<B, Const<0>>) {
            return Const<1>{};
        }
        else if constexpr (std::is_same_v<B, Const<1>>) {
            return a;
        }
        else {
            return Pow<A, B>{};
        }
    }

    template <class A, double (*f1)(double), double (*f2)(double), int phase>
    struct Circler {
        static constexpr A a;
        constexpr auto operator()(auto&&... x) const {
            if constexpr (phase == 0 || phase == 1) {
                return f1(a(x...));
            }
            else {
                return -f1(a(x...));
            }
        }

        constexpr auto derive() const {
            return internal::mul(Circler<A, f2, f1, (phase + 1) % 4>{}, a.derive());
        }

        constexpr auto print(auto&& out) const {
            if constexpr (phase == 2 || phase == 3) {
                strutil::append(out, "-");
            }
            if constexpr (phase == 0 || phase == 2) {
                strutil::append(out, "sin");
            }
            else {
                strutil::append(out, "cos");
            }
            strutil::append(out, "(");
            a.print(out);
            strutil::append(out, ")");
        }
    };

    template <class A>
    constexpr auto sin(A x) {
        return Circler<A, std::sin, std::cos, 0>{};
    }
    constexpr auto cos(auto x) {
        return sin(x).derive();
    }

    template <class A>
    constexpr auto exp(A x) {
        return Pow<decltype(e), A>{};
    }

}  // namespace utils::math
