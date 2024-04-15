/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <view/iovec.h>
#include "contract.h"
namespace futils {
    namespace binary {
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

        /*
        template <class C>
        struct ReadContract {
           private:
            size_t request = 0;
            view::basic_rvec<C>& buf;
            size_t index = 0;

           public:
            constexpr ReadContract(view::basic_rvec<C>& buf, size_t index, size_t request)
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

            constexpr view::basic_rvec<C> buffer() const noexcept {
                return buf;
            }

            constexpr bool set_new_buffer(view::basic_rvec<C> new_buf) noexcept {
                if (new_buf.size() < buf.size()) {
                    return false;
                }
                buf = new_buf;
                return true;
            }
        };

        template <class C>
        struct DiscardContract {
           private:
            size_t& index;
            size_t require_drop_n = 0;
            view::basic_rvec<C>& buf;

           public:
            constexpr DiscardContract(view::basic_rvec<C>& buf, size_t& index, size_t require_drop)
                : buf(buf), index(index), require_drop_n(require_drop) {
                assert(index < buf.size() && require_drop <= index);
            }

            constexpr view::basic_rvec<C> buffer() const noexcept {
                return buf;
            }

            constexpr size_t require_drop() const noexcept {
                return require_drop_n;
            }

            // direct_drop() drops n bytes from the buffer directly using memory copy
            // this operation may be unsafe because buf.data() may be read-only memory
            // caller must ensure that buf is writable
            constexpr bool direct_drop(size_t drop, view::basic_rvec<C>& old_range) {
                if (drop > require_drop_n) {
                    return false;
                }
                auto wbuf = view::basic_wvec<C>(const_cast<C*>(buf.data()), buf.size());
                constexpr auto copy_ = view::make_copy_fn<C>();
                copy_(wbuf, buf.substr(drop));
                old_range = buf;
                buf = buf.substr(0, buf.size() - drop);
                index -= drop;
                require_drop_n -= drop;
                return true;
            }

            // set_new_buffer() drops n bytes from the buffer and set new buffer
            constexpr bool set_new_buffer(view::basic_rvec<C> new_buf, size_t drop) noexcept {
                if (drop > require_drop_n) {
                    return false;
                }
                require_drop_n -= drop;
                index -= drop;
                buf = new_buf;
            }
        };*/

        template <class C>
        using ReadContract = internal::IncreaseContract<C, view::basic_rvec>;
        template <class C>
        using DiscardContract = internal::DecreaseContract<C, view::basic_rvec>;

        template <class C>
        struct ReadStreamHandler {
            // must not be null
            bool (*empty)(void* ctx, size_t offset) = nullptr;
            // maybe null
            void (*init)(void* ctx, view::basic_rvec<C>& buf) = nullptr;
            // maybe null
            // pre contract: `index < buf.size() && (request >= buf.size() - index)`
            // post contract: `(buf.size() - index) >= request` if return true. `buf.size() >= prev buf.size()`
            void (*read)(void* ctx, ReadContract<C> c) = nullptr;
            // maybe null
            void (*discard)(void* ctx, DiscardContract<C> c) = nullptr;
            // maybe null
            void (*destroy)(void* ctx, view::basic_rvec<C> buf) = nullptr;
            // maybe null
            // if clone is null, ctx and buf will shallow copied
            // implementation MUST guarantee safety of shallow copy if clone is null
            void (*clone)(void* ctx, view::basic_rvec<C> buf, void*& new_ctx, view::basic_rvec<C>& new_buf) = nullptr;
        };

        template <class C>
        struct basic_reader {
           private:
            view::basic_rvec<C> r;
            size_t index = 0;
            const ReadStreamHandler<C>* handler = nullptr;
            void* ctx = nullptr;

            // for clone
            constexpr basic_reader(view::basic_rvec<C> r, size_t index)
                : r(r), index(index) {}

            constexpr basic_reader(view::basic_rvec<C> r, size_t index, const ReadStreamHandler<C>* handler, void* ctx)
                : r(r), index(index), handler(handler), ctx(ctx) {}

            static constexpr void fill_null(basic_reader& r) noexcept {
                r.r = view::basic_rvec<C>();
                r.index = 0;
                r.handler = nullptr;
                r.ctx = nullptr;
            }

            constexpr void destroy() {
                if (handler && handler->destroy) {
                    handler->destroy(this->ctx, r);
                }
            }

            constexpr void init() {
                if (handler && handler->init) {
                    handler->init(this->ctx, r);
                }
            }

            constexpr void stream_read(size_t n) {
                if (handler && handler->read) {
                    if (n > r.size() - index) {
                        handler->read(this->ctx, ReadContract<C>(r, index, n));
                    }
                }
            }

            constexpr void offset_internal(size_t add) {
                if (r.size() - index < add) {
                    index = r.size();
                }
                else {
                    index += add;
                }
            }

            constexpr view::basic_rvec<C> read_best_internal(size_t n) noexcept {
                stream_read(n);
                auto result = r.substr(index, n);
                offset_internal(n);
                return result;
            }

            constexpr bool read_internal(view::basic_rvec<C>& data, size_t n) noexcept {
                size_t before = index;
                data = read_best_internal(n);
                if (data.size() != n) {
                    index = before;
                    return false;
                }
                return true;
            }

            constexpr std::pair<view::basic_rvec<C>, bool> read_internal(size_t n) noexcept {
                view::basic_rvec<C> data;
                bool ok = read_internal(data, n);
                return {data, ok};
            }

           public:
            constexpr basic_reader(view::basic_rvec<C> r)
                : r(r) {}

            constexpr basic_reader(const ReadStreamHandler<C>* handler, void* ctx, view::basic_rvec<C> r = {})
                : r(r), handler(handler), ctx(ctx) {
                init();
            }

            constexpr basic_reader(const basic_reader& o) = delete;
            constexpr basic_reader& operator=(const basic_reader& o) = delete;

            constexpr basic_reader(basic_reader&& o) noexcept
                : r(o.r), index(o.index), handler(o.handler), ctx(o.ctx) {
                fill_null(o);
            }

            constexpr basic_reader& operator=(basic_reader&& o) noexcept {
                if (this == &o) {
                    return *this;
                }
                destroy();
                r = o.r;
                index = o.index;
                handler = o.handler;
                ctx = o.ctx;
                fill_null(o);
                return *this;
            }

            constexpr basic_reader<C> clone() const {
                if (!handler) {
                    return basic_reader(r, index);
                }
                if (!handler->clone) {
                    return basic_reader(r, index, handler, ctx);
                }
                view::basic_rvec<C> new_buf;
                void* new_ctx = nullptr;
                handler->clone(ctx, r, new_ctx, new_buf);
                return basic_reader(handler, new_ctx, new_buf);
            }

            constexpr ~basic_reader() {
                destroy();
            }

            constexpr void reset(size_t pos = 0) {
                if (r.size() < pos) {
                    stream_read(pos - r.size());
                }
                if (r.size() < pos) {
                    index = r.size();
                    return;
                }
                index = pos;
            }

            constexpr void reset_buffer(view::basic_rvec<C> o, size_t pos = 0) {
                destroy();
                r = o;
                handler = nullptr;
                ctx = nullptr;
                reset(pos);
            }

            constexpr void reset_buffer(const ReadStreamHandler<C>* h, void* c, view::basic_rvec<C> o = {}, size_t pos = 0) {
                destroy();
                r = o;
                handler = h;
                ctx = c;
                init();
                reset(pos);
            }

            constexpr bool empty() const {
                return r.size() == index && (!handler || handler->empty(this->ctx, index));
            }

            // drop() drops read data from the beginning of the buffer
            constexpr bool discard() {
                return discard(index);
            }

            // drop() drops n bytes from the beginning of the buffer
            // n must be less than or equal to the current offset()
            constexpr bool discard(size_t n) {
                if (!handler || !handler->discard || n > index) {
                    return false;
                }
                handler->discard(this->ctx, DiscardContract<C>(r, index, n));
                return true;
            }

            // remain() returns remaining data
            // if handler is set,
            // This returns the data currently read on the buffer, but it does not represent all remaining data
            constexpr view::basic_rvec<C> remain() const {
                return r.substr(index);
            }

            constexpr void offset(size_t add) {
                stream_read(add);
                offset_internal(add);
            }

            // load_stream() loads n bytes from the stream to buffer
            // if the buffer is enough, this function returns true otherwise false
            // this function can be called both on stream mode and non-stream mode
            constexpr bool load_stream(size_t n) {
                stream_read(n);
                return remain().size() >= n;
            }

            [[nodiscard]] constexpr size_t offset() const noexcept {
                return index;
            }

            // read() returns already read span
            constexpr view::basic_rvec<C> read() const noexcept {
                return r.substr(0, index);
            }

            // read_best() reads maximum n bytes
            // on stream mode, this function always return empty view
            constexpr view::basic_rvec<C> read_best(size_t n) noexcept {
                if (is_stream()) {
                    return view::basic_rvec<C>();
                }
                return read_best_internal(n);
            }

            // read_best_direct() reads maximum n bytes
            // this function is same as read_best() but read data directly even on stream mode
            // caller MUST NOT expect that return value can be available when other operations on the reader after
            constexpr view::basic_rvec<C> read_best_direct(size_t n) noexcept {
                return read_best_internal(n);
            }

            // read_best() reads maximum buf.size() bytes and returns true if read exactly buf.size() bytes
            constexpr bool read_best(view::basic_wvec<C> buf) noexcept {
                constexpr auto copy_ = view::make_copy_fn<C>();
                return copy_(buf, read_best_internal(buf.size())) == 0;
            }

            template <ResizableBuffer<C> B>
            constexpr bool read_best(B& buf, size_t n) noexcept(noexcept(buf.resize(0))) {
                if constexpr (has_max_size<B>) {
                    if (buf.max_size() < r.size() - index) {
                        return false;
                    }
                }
                auto data = read_best_internal(n);
                if constexpr (resize_returns_bool<B>) {
                    if (!buf.resize(data.size())) {
                        return false;
                    }
                }
                else {
                    buf.resize(data.size());
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                return copy_(buf, data) == 0;
            }

            constexpr bool is_stream() const noexcept {
                return handler != nullptr;
            }

            // read() reads n bytes strictly or doesn't read if not enough remaining data
            // on stream mode, this function always return false
            constexpr bool read(view::basic_rvec<C>& data, size_t n) noexcept {
                if (is_stream()) {
                    return false;
                }
                return read_internal(data, n);
            }

            // read_direct() reads n bytes strictly or doesn't read if not enough remaining data
            // this function is same as read() but read data directly even on stream mode
            // caller MUST NOT expect that data can be available when other operations on the reader after
            constexpr bool read_direct(view::basic_rvec<C>& data, size_t n) noexcept {
                return read_internal(data, n);
            }

            // read() reads n bytes strictly or doesn't read if not enough remaining data.
            // on stream mode, this function always return false
            constexpr std::pair<view::basic_rvec<C>, bool> read(size_t n) noexcept {
                if (is_stream()) {
                    return {view::basic_rvec<C>(), false};
                }
                return read_internal(n);
            }

            // read_direct() reads n bytes strictly or doesn't read if not enough remaining data.
            // this function is same as read() but read data directly even on stream mode
            // caller MUST NOT expect that data can be available when other operations on the reader after
            constexpr std::pair<view::basic_rvec<C>, bool> read_direct(size_t n) noexcept {
                return read_internal(n);
            }

            // read() reads buf.size() bytes strictly or doesn't read if not enough remaining data.
            // if read, copy data to buf
            constexpr bool read(view::basic_wvec<C> buf) noexcept {
                auto [data, ok] = read_internal(buf.size());
                if (!ok) {
                    return false;
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                return copy_(buf, data) == 0;
            }

            template <ResizableBuffer<C> B>
            constexpr bool read(B& buf, size_t n) noexcept(noexcept(buf.resize(0))) {
                if constexpr (has_max_size<B>) {
                    if (n > buf.max_size()) {
                        return false;
                    }
                }
                auto [data, ok] = read_internal(n);
                if (!ok) {
                    return false;
                }
                if constexpr (resize_returns_bool<B>) {
                    if (!buf.resize(data.size())) {
                        return false;
                    }
                }
                else {
                    buf.resize(data.size());
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                return copy_(buf, data) == 0;
            }

            // top() returns the first element of the buffer
            constexpr C top() noexcept {
                return load_stream(1) ? remain().data()[0] : 0;
            }
        };

        using reader = basic_reader<byte>;

        namespace test {
            struct ArrayStreamReader {
                static constexpr auto N = 100;
                byte buffer[N]{};
                size_t used = 0;

                constexpr static bool empty(void* ctx, size_t offset) {
                    auto self = static_cast<ArrayStreamReader*>(ctx);
                    return offset == N;
                }

                constexpr static void read(void* ctx, ReadContract<byte> c) {
                    auto len = c.least_new_buffer_size();
                    if (len > N) {
                        len = N;
                    }
                    auto self = static_cast<ArrayStreamReader*>(ctx);
                    c.set_new_buffer(view::basic_rvec<byte>(self->buffer, len));
                }

                static constexpr ReadStreamHandler<byte> handler = {
                    .empty = empty,
                    .read = read,
                };
            };

            inline bool test_read_stream() {
                ArrayStreamReader stream;
                binary::reader r(&ArrayStreamReader::handler, &stream);
                if (!r.remain().empty()) {
                    throw "failed to initialize";
                }
                byte buffer[10];
                if (!r.read(buffer)) {
                    throw "failed to read";
                }
                if (r.read().size() != 10) {
                    throw "failed to read";
                }
                if (r.empty()) {
                    throw "failed to read";
                }
                if (!r.load_stream(10)) {
                    throw "failed to read";
                }
                if (r.remain().size() != 10) {
                    throw "failed to read";
                }
                return true;
            }

            // static_assert(test_stream());

        }  // namespace test
    }      // namespace binary
}  // namespace futils
