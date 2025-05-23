/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <view/iovec.h>

namespace futils::binary::internal {
    template <class C, template <class T> class Vec>
    struct IncreaseContract {
       private:
        size_t request = 0;
        Vec<C>& buf;
        size_t index = 0;

       public:
        constexpr IncreaseContract(Vec<C>& buf, size_t index, size_t request)
            : buf(buf), index(index), request(request) {
            assert(index <= buf.size() && (request > buf.size() - index));
        }

        constexpr size_t remaining() const noexcept {
            return buf.size() - index;
        }

        constexpr size_t requested() const noexcept {
            return request;
        }

        constexpr size_t offset() const noexcept {
            return index;
        }

        constexpr size_t least_requested() const noexcept {
            return request - remaining();
        }

        constexpr size_t least_new_buffer_size() const noexcept {
            return buf.size() + least_requested();
        }

        constexpr Vec<C> buffer() const noexcept {
            return buf;
        }

        constexpr bool set_new_buffer(Vec<C> new_buf) noexcept {
            if (new_buf.size() < buf.size()) {
                return false;
            }
            buf = new_buf;
            return true;
        }
    };

    template <class C, template <class T> class Vec>
    struct DecreaseContract {
       private:
        size_t& index;
        size_t require_drop_n = 0;
        Vec<C>& buf;

       public:
        constexpr DecreaseContract(Vec<C>& buf, size_t& index, size_t require_drop)
            : buf(buf), index(index), require_drop_n(require_drop) {
            assert(index <= buf.size() && require_drop <= index);
        }

        constexpr Vec<C> buffer() const noexcept {
            return buf;
        }

        constexpr size_t require_drop() const noexcept {
            return require_drop_n;
        }

        // direct_drop() drops n bytes from the buffer directly using memory copy
        // this operation may be unsafe because buf.data() may be read-only memory
        // caller must ensure that buf is writable
        constexpr bool direct_drop(size_t drop, Vec<C>* old_range = nullptr) {
            if (drop > require_drop_n) {
                return false;
            }
            auto wbuf = view::basic_wvec<C>(const_cast<C*>(buf.data()), buf.size());
            constexpr auto copy_ = view::make_copy_fn<C>();
            copy_(wbuf, buf.substr(drop));
            if (old_range) {
                *old_range = buf;
            }
            buf = buf.substr(0, buf.size() - drop);
            index -= drop;
            require_drop_n -= drop;
            return true;
        }

        // replace_buffer() replaces the buffer with new_buf
        // new_buf must have the same size as the current buffer
        // and the buffer contents must be as same as the current buffer
        constexpr bool replace_buffer(Vec<C> new_buf) {
            if (buf.size() != new_buf.size()) {
                return false;
            }
            buf = new_buf;
            return true;
        }

        // set_new_buffer() drops n bytes from the buffer and set new buffer
        constexpr bool set_new_buffer(Vec<C> new_buf, size_t drop) noexcept {
            if (drop > require_drop_n) {
                return false;
            }
            require_drop_n -= drop;
            index -= drop;
            buf = new_buf;
            return true;
        }
    };

    template <class T, class C>
    concept ResizableBuffer = requires(T& n, size_t s) {
        { n.resize(s) };
        { view::basic_wvec<C>(n) };
    };

    template <class T>
    concept has_max_size = requires(T t) {
        { t.max_size() } -> std::convertible_to<size_t>;
    };

    template <class T>
    concept resize_returns_bool = requires(T t, size_t s) {
        { t.resize(s) } -> std::convertible_to<bool>;
    };

    template <class T>
    constexpr bool resize_buffer(T& buf, size_t size) {
        if constexpr (resize_returns_bool<T>) {
            return buf.resize(size);
        }
        else {
            buf.resize(size);
            return true;
        }
    }

    template <class T, class C>
    concept ArrayReadableBuffer = (!view::internal::is_const_data<T, C>) && requires(T t) {
        { t[0] } -> std::convertible_to<C>;
        { t.size() } -> std::convertible_to<size_t>;
    };

}  // namespace futils::binary::internal
