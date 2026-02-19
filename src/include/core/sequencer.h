/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// sequencer - buffer sequencer
#pragma once

#include <core/buffer.h>

namespace futils {
    template <class T>
    struct Sequencer {
        template <class S = T>
        using buf_t = BufferType<S>;
        using char_type = std::remove_cvref_t<typename buf_t<>::char_type>;
        Buffer<T> buf;
        size_t rptr = 0;

        template <class U, helper_disable_self(Sequencer, U)>
        constexpr Sequencer(U&& args)
            : buf(std::forward<U>(args)) {}

        constexpr Sequencer(T ptr, size_t size)
            requires std::is_pointer_v<T>
            : buf(ptr, size) {}

        constexpr Sequencer(T begin, T end)
            requires std::is_pointer_v<T>
            : buf(begin, end - begin) {}

        constexpr Sequencer(Sequencer&& in)
            : buf(std::move(in.buf)), rptr(in.rptr) {}

       private:
        template <class String, class F>
        constexpr size_t match_impl(Buffer<String>& tmp, F&& f) {
            if (buf.size() <= rptr) {
                return 0;
            }
            size_t offset = 0;
            size_t maxlen = buf.size() - rptr > tmp.size() ? tmp.size() : buf.size() - rptr;
            while (true) {
                if (offset >= maxlen) {
                    return offset;
                }
                if (!f(buf.at(rptr + offset), tmp.at(offset))) {
                    return offset;
                }
                offset++;
            }
        }

       public:
        template <class String>
        using compare_type = bool (*)(typename buf_t<>::char_type, typename buf_t<String&>::char_type);

        template <class String>
        constexpr static auto default_compare() {
            return [](typename buf_t<>::char_type a, typename buf_t<String&>::char_type b) {
                return a == b;
            };
        }

        constexpr size_t size() const {
            return buf.size();
        }

        constexpr size_t remain() const {
            return buf.size() - rptr;
        }

        template <class String, class F = compare_type<String>>
        constexpr size_t part(String&& cmp, F&& f = default_compare<String>()) {
            Buffer<typename buf_t<String&>::type> tmp(cmp);
            return match_impl(tmp, std::forward<F>(f));
        }

        template <class String, class F = compare_type<String>>
        constexpr bool match(String&& cmp, F&& f = default_compare<String>()) {
            Buffer<typename buf_t<String&>::type> tmp(cmp);
            size_t offset = match_impl(tmp, std::forward<F>(f));
            return offset == tmp.size();
        }

        template <class String, class F = compare_type<String>>
        constexpr size_t match_n(String&& cmp, F&& f = default_compare<String>()) {
            Buffer<typename buf_t<String&>::type> tmp(cmp);
            size_t offset = match_impl(tmp, std::forward<F>(f));
            return offset == tmp.size() ? offset : 0;
        }

        template <class String, class F = compare_type<String>>
        constexpr bool seek_if(String&& cmp, F&& f = default_compare<String>()) {
            Buffer<typename buf_t<String&>::type> tmp(cmp);
            size_t offset = match_impl(tmp, std::forward<F>(f));
            if (offset == tmp.size()) {
                rptr += offset;
                return true;
            }
            return false;
        }

        template <class Char>
        constexpr bool consume_if(Char c) {
            if (eos()) {
                return false;
            }
            if (c != buf.at(rptr)) {
                return false;
            }
            rptr++;
            return true;
        }

        constexpr char_type current(std::int64_t offset = 0) const {
            if (!buf.in_range(rptr + offset)) {
                return char_type();
            }
            return buf.at(rptr + offset);
        }

        constexpr bool consume(size_t offset = 1) {
            if (!buf.in_range(rptr + offset - 1)) {
                return false;
            }
            rptr += offset;
            return true;
        }

        constexpr bool backto(size_t offset = 1) {
            if (offset > rptr) {
                return false;
            }
            rptr -= offset;
            return true;
        }

        constexpr bool seek(size_t position) {
            if (position != 0 && !buf.in_range(position)) {
                return false;
            }
            rptr = position;
            return true;
        }

        constexpr void seekend() {
            rptr = buf.size();
        }

        constexpr bool eos() const {
            return buf.in_range(rptr) == false;
        }
    };

    template <class C>
    Sequencer(C*) -> Sequencer<C*>;

    template <class Buf>
    Sequencer(const Buf&) -> Sequencer<Buf>;

    template <class Buf>
    Sequencer(Buf&&) -> Sequencer<Buf>;

    template <class String>
    constexpr Sequencer<buffer_t<String&>> make_ref_seq(String&& v) {
        return Sequencer<buffer_t<String&>>{v};
    }

    template <class String>
    constexpr Sequencer<std::decay_t<String>> make_cpy_seq(const String& v) {
        return Sequencer<std::decay_t<String>>{v};
    }

}  // namespace futils
