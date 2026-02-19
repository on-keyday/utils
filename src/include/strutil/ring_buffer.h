/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
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
            return head - tail;
        }
        constexpr bool empty() const {
            return head == tail;
        }
        constexpr bool full() const {
            return size() == capacity();
        }

        constexpr C& front() {
            return buf[tail_index()];
        }

        constexpr const C& front() const {
            return buf[tail_index()];
        }

        constexpr C& back() {
            return buf[head_index(-1)];
        }

        constexpr const C& back() const {
            return buf[head_index(-1)];
        }

        constexpr void push(C value) {
            assert(!full());
            if (full()) {
                return;
            }
            if (empty()) {
                clear();
            }
            buf[head_index()] = value;
            head++;
        }

        constexpr void pop() {
            assert(!empty());
            if (empty()) {
                return;
            }
            tail++;
        }

        constexpr size_t head_index(int offset = 0) const {
            return (head + offset) % N;
        }

        constexpr size_t tail_index(int offset = 0) const {
            return (tail + offset) % N;
        }

        constexpr C& operator[](size_t index) {
            return buf[tail_index(index)];
        }
        constexpr const C& operator[](size_t index) const {
            return buf[tail_index(index)];
        }

        constexpr void clear() {
            head = tail = 0;
        }

        constexpr void copy_to(C (&dest)[N]) const {
            for (size_t i = 0; i < size(); ++i) {
                dest[i] = buf[tail_index(i)];
            }
            dest[size()] = '\0';
        }

        constexpr C* head_ptr() {
            return buf + head_index();
        }

        constexpr const C* head_ptr() const {
            return buf + head_index();
        }

        constexpr C* tail_ptr() {
            return buf + tail_index();
        }

        constexpr const C* tail_ptr() const {
            return buf + tail_index();
        }

        constexpr bool liner() const {
            return tail_ptr() < head_ptr() || (head_ptr() == buf && tail_index(size() - 1) == N - 1);
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
                throw "full error";
            }
            if (b.size() != 3) {
                throw "size error";
            }
            if (b.front() != 'a') {
                throw "front error";
            }
            if (b.back() != 'c') {
                throw "back error";
            }
            b.pop();
            // | |b|c| |
            //    t   h
            if (b.size() != 2) {
                throw "size error";
            }
            if (b.front() != 'b') {
                throw "front error";
            }
            if (b.back() != 'c') {
                throw "back error";
            }
            b.push('d');
            // | |b|c|d|
            //  h t
            if (b.size() != 3) {
                throw "size error";
            }
            if (b.front() != 'b') {
                throw "front error";
            }
            if (b.back() != 'd') {
                throw "back error";
            }
            auto ptr = b.tail_ptr();
            if (ptr[0] != 'b' || ptr[1] != 'c' || ptr[2] != 'd') {
                throw "tail_ptr error";
            }
            if (!b.liner()) {
                throw "liner error";
            }
            char dest[4];
            b.copy_to(dest);
            if (dest[0] != 'b' || dest[1] != 'c' || dest[2] != 'd' || dest[3] != '\0') {
                throw "copy_to error";
            }
            ptr = b.tail_ptr();
            if (ptr[0] != 'b' || ptr[1] != 'c' || ptr[2] != 'd') {
                throw "tail_ptr error";
            }
            b.pop();
            b.push('e');
            // |e| |c|d|
            //    h t
            if (b.size() != 3) {
                throw "size error";
            }
            if (b.front() != 'c') {
                throw "front error";
            }
            if (b.back() != 'e') {
                throw "back error";
            }
            if (b.liner()) {
                throw "liner error";
            }
            b.copy_to(dest);
            if (dest[0] != 'c' || dest[1] != 'd' || dest[2] != 'e' || dest[3] != '\0') {
                throw "copy_to error";
            }
            b.pop();
            b.pop();
            b.pop();
            // | | | | |
            //  h
            //  t
            if (!b.empty()) {
                throw "empty error";
            }
            b.push('e');
            if (b.size() != 1) {
                throw "size error";
            }
            if (b.front() != 'e') {
                throw "front error";
            }
            if (b.back() != 'e') {
                throw "back error";
            }
            return true;
        }

        static_assert(test_ring_buffer());
    }  // namespace test
}  // namespace futils::strutil
