/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <view/iovec.h>
#include <binary/contract.h>
namespace futils {
    namespace binary {

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
            // direct_read is optional. when use this, buffer will not be used
            void (*direct_buffer)(void* ctx, view::basic_rvec<C>& buf, size_t expected) = nullptr;
            size_t (*direct_read)(void* ctx, view::basic_wvec<C> w, size_t expected) = nullptr;
            void (*direct_read_view)(void* ctx, view::basic_rvec<C>& w, size_t expected) = nullptr;
            // maybe null
            void (*discard)(void* ctx, DiscardContract<C> c) = nullptr;
            // maybe null
            void (*destroy)(void* ctx, DiscardContract<C> c) = nullptr;
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
                    handler->destroy(this->ctx, DiscardContract<C>(r, index, index));
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
                view::basic_rvec<C> result;
                if (auto [called, _] = direct_read_view(result, n); called) {
                    return result;
                }
                stream_read(n);
                result = r.substr(index, n);
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

            constexpr std::pair<bool, bool> direct_read(view::basic_wvec<C> w, bool is_best) {
                if (handler && handler->direct_read) {
                    return {true, handler->direct_read(ctx, w, w.size()) == w.size()};
                }
                if (handler && handler->direct_read_view) {
                    view::basic_rvec<C> tmp;
                    handler->direct_read_view(ctx, tmp, w.size());
                    if (!is_best && tmp.size() != w.size()) {
                        return {true, false};
                    }
                    constexpr auto copy = view::make_copy_fn<C>();
                    copy(w, tmp);
                    return {true, tmp.size() == w.size()};
                }
                return {false, false};
            }

            constexpr std::pair<bool, bool> direct_read_view(view::basic_rvec<C>& w, size_t n) {
                if (handler && handler->direct_read_view) {
                    handler->direct_read_view(ctx, w, n);
                    return {true, w.size() == n};
                }
                if (handler && handler->direct_read) {
                    if (!handler->direct_buffer) {
                        return {true, false};
                    }
                    view::basic_wvec<C> tmp;
                    handler->direct_buffer(ctx, tmp, n);
                    if (tmp.size() != n) {
                        return {true, false};
                    }
                    auto read_size = handler->direct_read(ctx, tmp, n);
                    w = tmp;
                    return {true, read_size == n};
                }
                return {false, false};
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

            constexpr void* context() const noexcept {
                return ctx;
            }

            constexpr const ReadStreamHandler<C>* stream_handler() const noexcept {
                return handler;
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
                if (auto [called, ok] = direct_read(buf, true); called) {
                    return ok;
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                return copy_(buf, read_best_internal(buf.size())) == 0;
            }

            template <internal::ResizableBuffer<C> B>
            constexpr bool read_best(B& buf, size_t n) noexcept(noexcept(buf.resize(0))) {
                if constexpr (internal::has_max_size<B>) {
                    if (buf.max_size() < r.size() - index) {
                        return false;
                    }
                }
                auto save = index;
                auto data = read_best_internal(n);
                if (!internal::resize_buffer(buf, data.size())) {
                    index = save;
                    return false;
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                return copy_(buf, data) == 0;
            }

            constexpr bool is_stream() const noexcept {
                return handler != nullptr;
            }

            constexpr bool is_direct_stream() const noexcept {
                return handler && handler->direct_read;
            }

            constexpr bool is_direct_stream_view() const noexcept {
                return handler && handler->direct_read_view;
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
                if (auto [called, ok] = direct_read(buf, false); called) {
                    return ok;
                }
                auto [data, ok] = read_internal(buf.size());
                if (!ok) {
                    return false;
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                return copy_(buf, data) == 0;
            }

            template <internal::ResizableBuffer<C> B>
            constexpr bool read(B& buf, size_t n) noexcept(noexcept(buf.resize(0))) {
                if constexpr (internal::has_max_size<B>) {
                    if (n > buf.max_size()) {
                        return false;
                    }
                }
                auto save = index;
                auto [data, ok] = read_internal(n);
                if (!ok) {
                    return false;
                }
                if (!internal::resize_buffer(buf, data.size())) {
                    index = save;
                    return false;
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                return copy_(buf, data) == 0;
            }

           private:
            template <internal::ResizableBuffer<C> B>
            constexpr bool read_until_eof_no_stream(B& buf) {
                auto save = index;
                auto to_read = remain().size();
                auto direct = read_best_internal(to_read);
                assert(direct.size() == to_read);
                if (!internal::resize_buffer(buf, to_read)) {
                    index = save;
                    return false;
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                copy_(buf, direct);
                return true;
            }

           public:
            constexpr bool read_until_eof(view::basic_rvec<C>& data) {
                if (is_stream()) {
                    return false;
                }
                data = read_best_internal(remain().size());
                return true;
            }

            template <internal::ResizableBuffer<C> B>
            constexpr bool read_until_eof(B& buf, size_t rate = 1024, bool should_discard = false) {
                if (!is_stream()) {
                    return read_until_eof_no_stream(buf);
                }
                while (true) {
                    auto data = read_best_internal(rate);
                    if (data.empty()) {
                        return true;
                    }
                    if (!internal::resize_buffer(buf, buf.size() + data.size())) {
                        return false;
                    }
                    constexpr auto copy_ = view::make_copy_fn<C>();
                    copy_(view::basic_wvec<C>(buf).substr(buf.size() - data.size()), data);
                    if (should_discard) {
                        discard();
                    }
                }
            }

            // top() returns the first element of the buffer
            constexpr C top() noexcept {
                return load_stream(1) ? remain().data()[0] : 0;
            }
        };

        using reader = basic_reader<byte>;

        template <class T, class C = byte>
        struct ReadStreamingBuffer {
           private:
            T buffer;
            size_t offset = 0;
            // clang-format off
            static constexpr ReadStreamHandler handler = {
                .empty = [](void* t, size_t offset) {
                    auto self = static_cast<T*>(t);
                    return offset == self->size(); 
                },
                .discard = [](void* t, DiscardContract<C> c) {
                    auto self = static_cast<T*>(t);
                    auto drop = c.require_drop(); 
                    c.direct_drop(drop);
                    self->resize(c.buffer().size());
                    c.replace_buffer(*self); 
                },
            };
            // clang-format on

           public:
            constexpr auto stream(auto&& cb) {
                basic_reader<C> r{&handler, &buffer, buffer};
                r.reset(offset);
                // exception safe
                // to prevent include <helper/defer.h>
                struct L {
                    basic_reader<C>* r;
                    size_t* offset;
                    L(size_t* offset, basic_reader<C>* r)
                        : offset(offset), r(r) {}
                    ~L() {
                        *offset = r->offset();
                    }
                } l{&offset, &r};
                return cb(r);
            }
        };

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
    }  // namespace binary
}  // namespace futils
