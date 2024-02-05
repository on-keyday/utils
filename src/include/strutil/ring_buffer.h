/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>
#include <cassert>

namespace futils::strutil {

    template <typename C, size_t N>
    struct FixedRingBuffer {
       private:
        C buf[N];
        size_t head = 0;
        size_t tail = 0;

       public:
        constexpr size_t capacity() const {
            return N - 1;
        }
        constexpr size_t size() const {
            return (head - tail) % N;
        }
        constexpr bool empty() const {
            return head == tail;
        }
        constexpr bool full() const {
            return size() == capacity();
        }

        constexpr C& front() {
            return buf[tail];
        }

        constexpr const C& front() const {
            return buf[tail];
        }

        constexpr C& back() {
            return buf[(head - 1) % N];
        }

        constexpr const C& back() const {
            return buf[(head - 1) % N];
        }

        constexpr void push(C value) {
            assert(!full());
            if (full()) {
                return;
            }
            buf[head] = value;
            head = (head + 1) % N;
            if (head == tail) {
                tail = (tail + 1) % N;
            }
        }

        constexpr void pop() {
            assert(!empty());
            if (empty()) {
                return;
            }
            tail = (tail + 1) % N;
        }

        constexpr C& operator[](size_t index) {
            return buf[(tail + index) % N];
        }
        constexpr const C& operator[](size_t index) const {
            return buf[(tail + index) % N];
        }

        constexpr void clear() {
            head = tail = 0;
        }

        constexpr void copy_to(C (&dest)[N]) const {
            for (size_t i = 0; i < size(); ++i) {
                dest[i] = buf[(tail + i) % N];
            }
            dest[size()] = '\0';
        }

        constexpr C* head_ptr() {
            return buf + head;
        }

        constexpr const C* head_ptr() const {
            return buf + head;
        }

        constexpr C* tail_ptr() {
            return buf + tail;
        }

        constexpr const C* tail_ptr() const {
            return buf + tail;
        }

        constexpr bool liner() const {
            return tail + size() <= N;
        }
    };

    template <class C, size_t N, class J, class A>
    constexpr void buffered_action(FixedRingBuffer<C, N>& b, C to_add, J&& judge, A&& action) {
        if (b.full()) {
            if (b.liner()) {
                action(b.tail_ptr(), b.size());
            }
            else {
                C dest[N];
                b.copy_to(dest);
                action(dest, b.size());
            }
            b.clear();
        }
        b.push(to_add);
        if (judge(b)) {
            if (b.liner()) {
                action(b.tail_ptr(), b.size());
            }
            else {
                C dest[N];
                b.copy_to(dest);
                action(dest, b.size());
            }
            b.clear();
        }
    }

    namespace test {
        constexpr bool test_ring_buffer() {
            FixedRingBuffer<char, 4> b;
            b.push('a');
            b.push('b');
            b.push('c');
            // |a|b|c| |
            //  t     h
            if (!b.full()) {
                return false;
            }
            if (b.size() != 3) {
                return false;
            }
            if (b.front() != 'a') {
                return false;
            }
            if (b.back() != 'c') {
                return false;
            }
            b.pop();
            // | |b|c| |
            //    t   h
            if (b.size() != 2) {
                return false;
            }
            if (b.front() != 'b') {
                return false;
            }
            if (b.back() != 'c') {
                return false;
            }
            b.push('d');
            // | |b|c|d|
            //  h t
            if (b.size() != 3) {
                return false;
            }
            if (b.front() != 'b') {
                return false;
            }
            if (b.back() != 'd') {
                return false;
            }
            if (!b.liner()) {
                return false;
            }
            char dest[4];
            b.copy_to(dest);
            if (dest[0] != 'b' || dest[1] != 'c' || dest[2] != 'd' || dest[3] != '\0') {
                return false;
            }
            auto ptr = b.tail_ptr();
            if (ptr[0] != 'b' || ptr[1] != 'c' || ptr[2] != 'd') {
                return false;
            }
            b.pop();
            b.push('e');
            // |e| |c|d|
            //    h t
            if (b.size() != 3) {
                return false;
            }
            if (b.front() != 'c') {
                return false;
            }
            if (b.back() != 'e') {
                return false;
            }
            if (b.liner()) {
                return false;
            }
            b.copy_to(dest);
            if (dest[0] != 'c' || dest[1] != 'd' || dest[2] != 'e' || dest[3] != '\0') {
                return false;
            }
            b.pop();
            b.pop();
            b.pop();
            // | | | | |
            //  h
            //  t
            if (!b.empty()) {
                return false;
            }
            b.push('e');
            if (b.size() != 1) {
                return false;
            }
            if (b.front() != 'e') {
                return false;
            }
            if (b.back() != 'e') {
                return false;
            }
            return true;
        }

        static_assert(test_ring_buffer());
    }  // namespace test
}  // namespace futils::strutil
