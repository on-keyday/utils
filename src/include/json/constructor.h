/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "parser.h"
#include <escape/escape.h>

namespace futils::json {
    template <size_t n, class JSON>
    struct StaticStack {
       private:
        union {
            char dummy[sizeof(JSON) * n];
            JSON stack[n];
        };
        size_t size_ = 0;

       public:
        constexpr StaticStack() {}
        constexpr ~StaticStack() {
            for (size_t i = 0; i < size_; ++i) {
                stack[i].~JSON();
            }
        }

        constexpr void emplace_back(auto&&... args) {
            assert(size_ < n);
            new (&stack[size_++]) JSON(std::forward<decltype(args)>(args)...);
        }

        constexpr void push_back(JSON&& v) {
            assert(size_ < n);
            new (&stack[size_++]) JSON(std::move(v));
        }

        constexpr void push_back(const JSON& v) {
            assert(size_ < n);
            new (&stack[size_++]) JSON(v);
        }

        constexpr void pop_back() {
            assert(size_ > 0);
            stack[--size_].~JSON();
        }

        constexpr JSON& back() {
            assert(size_ > 0);
            return stack[size_ - 1];
        }

        constexpr size_t size() const {
            return size_;
        }

        constexpr size_t max_size() const {
            return n;
        }

        constexpr bool empty() const {
            return size_ == 0;
        }
    };

    struct StackObserver {
        void observe(ElementType, auto& s) {}
    };

    template <class JSON, class Stack, class Observer = StackObserver>
    struct JSONConstructor {
        Stack stack;
        size_t max_stack_size = 0;
        Observer observer;
        using String = typename JSON::string_t;

        constexpr void update_max_stack_size() {
            if (stack.size() > max_stack_size) {
                max_stack_size = stack.size();
            }
        }

        constexpr bool check_stack_size() {
            if constexpr (has_max_size<Stack>) {
                if (stack.size() >= stack.max_size()) {
                    return false;
                }
            }
            return true;
        }

        constexpr bool init_object(auto&) {
            if (!check_stack_size()) {
                return false;
            }
            stack.emplace_back();
            stack.back().init_obj();
            update_max_stack_size();
            observer.observe(ElementType::object, stack);
            return true;
        }

        constexpr bool init_array(auto& r) {
            if (!check_stack_size()) {
                return false;
            }
            stack.emplace_back();
            stack.back().init_array();
            update_max_stack_size();
            observer.observe(ElementType::array, stack);
            return true;
        }

        constexpr bool do_escape(String& out, auto begin, auto end) {
            auto seq = Sequencer(begin, end - begin);
            return escape::unescape_str(seq, out);
        }

        constexpr bool init_string(Reader auto& r) {
            if (!check_stack_size()) {
                return false;
            }
            auto range = r.range(ElementType::string);
            // remove quotes
            stack.emplace_back(String(std::ranges::begin(r.bytes) + range.first + 1, std::ranges::begin(r.bytes) + range.second - 1));
            update_max_stack_size();
            observer.observe(ElementType::string, stack);
            return true;
        }

        constexpr bool init_escaped_string(Reader auto& r) {
            if (!check_stack_size()) {
                return false;
            }
            auto range = r.range(ElementType::escaped_string);
            String str;
            if (!do_escape(str, std::ranges::begin(r.bytes) + range.first + 1, std::ranges::begin(r.bytes) + range.second - 1)) {
                return false;
            }
            stack.emplace_back(std::move(str));
            update_max_stack_size();
            observer.observe(ElementType::escaped_string, stack);
            return true;
        }

        constexpr bool init_key_string(Reader auto& r) {
            if (!check_stack_size()) {
                return false;
            }
            auto range = r.range(ElementType::key_string);
            // remove quotes
            stack.emplace_back(String(std::ranges::begin(r.bytes) + range.first + 1, std::ranges::begin(r.bytes) + range.second - 1));
            update_max_stack_size();
            observer.observe(ElementType::key_string, stack);
            return true;
        }

        constexpr bool init_escaped_key_string(Reader auto& r) {
            if (!check_stack_size()) {
                return false;
            }
            auto range = r.range(ElementType::escaped_key_string);
            String str;
            if (!do_escape(str, std::ranges::begin(r.bytes) + range.first + 1, std::ranges::begin(r.bytes) + range.second - 1)) {
                return false;
            }
            stack.emplace_back(std::move(str));
            update_max_stack_size();
            observer.observe(ElementType::escaped_key_string, stack);
            return true;
        }

        constexpr bool init_integer(Reader auto& r) {
            if (!check_stack_size()) {
                return false;
            }
            auto range = r.range(ElementType::integer);
            auto seq = Sequencer(std::ranges::begin(r.bytes) + range.first, std::ranges::begin(r.bytes) + range.second);
            auto sign = seq.consume_if('-');
            auto start_pos = seq.rptr;
            std::uint64_t num = 0;
            auto of = futils::number::parse_integer(seq, num, 10, futils::number::NumConfig<>{.allow_plus_sign = false});
            if (!of && of != futils::number::NumError::overflow) {
                return false;
            }
            bool overflow = of == futils::number::NumError::overflow;
            auto s64_max = std::numeric_limits<std::int64_t>::max();
            // should be floating point
            if (!overflow && num >= std::uint64_t(s64_max) + 1 && sign) {
                stack.emplace_back(-double(num));
            }
            else if (overflow) {
                seq.rptr = start_pos;
                double num2 = 0;
                if (!futils::number::parse_float(seq, num2, 10, futils::number::NumConfig<>{.allow_plus_sign = false})) {
                    return false;
                }
                if (sign) {
                    stack.emplace_back(-num2);
                }
                else {
                    stack.emplace_back(num2);
                }
            }
            else if (num > std::numeric_limits<std::int64_t>::max()) {
                stack.emplace_back(num);
            }
            else {
                if (sign) {
                    stack.emplace_back(-std::int64_t(num));
                }
                else {
                    stack.emplace_back(std::int64_t(num));
                }
            }
            update_max_stack_size();
            observer.observe(ElementType::integer, stack);
            return true;
        }

        constexpr bool init_floating(Reader auto& r) {
            if (!check_stack_size()) {
                return false;
            }
            auto range = r.range(ElementType::floating);
            double num = 0;
            auto seq = Sequencer(std::ranges::begin(r.bytes) + range.first, std::ranges::begin(r.bytes) + range.second);
            if (!futils::number::parse_float(seq, num)) {
                return false;
            }
            stack.emplace_back(num);
            update_max_stack_size();
            observer.observe(ElementType::floating, stack);
            return true;
        }

        constexpr bool init_null(Reader auto& r) {
            if (!check_stack_size()) {
                return false;
            }
            auto range = r.range(ElementType::null);
            stack.emplace_back(nullptr);
            update_max_stack_size();
            observer.observe(ElementType::null, stack);
            return true;
        }

        constexpr bool init_boolean(Reader auto& r) {
            if (!check_stack_size()) {
                return false;
            }
            auto range = r.range(ElementType::boolean);
            if (std::ranges::begin(r.bytes)[range.first] == 't') {
                stack.emplace_back(true);
            }
            else {
                stack.emplace_back(false);
            }
            update_max_stack_size();
            observer.observe(ElementType::boolean, stack);
            return true;
        }

        constexpr bool add_array_element(Reader auto& r) {
            auto back = std::move(stack.back());
            stack.pop_back();
            auto& s = stack.back().get_holder().init_as_array();
            s.push_back(std::move(back));
            observer.observe(ElementType::array_element, stack);
            return true;
        }

        constexpr bool add_object_field(Reader auto& r) {
            auto value = std::move(stack.back());
            stack.pop_back();
            auto key = std::move(stack.back());
            stack.pop_back();
            auto& s = stack.back().get_holder().init_as_object();
            s.emplace(std::move(key.get_holder().init_as_string()), std::move(value));
            observer.observe(ElementType::object_field, stack);
            return true;
        }
    };
}  // namespace futils::json
