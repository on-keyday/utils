/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// writer -writer
#pragma once
#include <view/iovec.h>
#include <binary/contract.h>

namespace futils {
    namespace binary {

        template <class C>
        using WriteContract = internal::IncreaseContract<C, view::basic_wvec>;
        template <class C>
        using CommitContract = internal::DecreaseContract<C, view::basic_wvec>;

        template <class C>
        struct WriteStreamHandler {
            // must not be null
            bool (*full)(void* ctx, size_t index) = nullptr;
            void (*init)(void* ctx, view::basic_wvec<C>& w) = nullptr;
            void (*write)(void* ctx, WriteContract<C>) = nullptr;
            // this is optional. when use this, buffer will not be used
            void (*direct_buffer)(void* ctx, view::basic_wvec<C>& w, size_t expected) = nullptr;
            bool (*direct_write)(void* ctx, view::basic_rvec<C> w, size_t count) = nullptr;
            void (*commit)(void* ctx, CommitContract<C>) = nullptr;
            // maybe null
            void (*destroy)(void* ctx, CommitContract<C>) = nullptr;
            // maybe null
            // if clone is null, ctx and buf will shallow copied
            // implementation MUST guarantee safety of shallow copy if clone is null
            void (*clone)(void* ctx, view::basic_wvec<C> w, void*& new_ctx, view::basic_wvec<C>& new_w) = nullptr;
        };

        template <class C, class T>
        struct WriteStreamHandlerT {
            WriteStreamHandler<C> base;
            using contract_type = T;
        };

        template <class C>
        struct basic_writer {
            using value_type = C;

           private:
            view::basic_wvec<C> w;
            size_t index = 0;
            const WriteStreamHandler<C>* handler = nullptr;
            void* ctx = nullptr;

            constexpr void init() {
                if (handler && handler->init) {
                    handler->init(ctx, w);
                }
            }

            constexpr void destroy() {
                if (handler && handler->destroy) {
                    handler->destroy(ctx, CommitContract<C>{w, index, index});
                }
            }

            constexpr void stream_write(size_t n) {
                if (handler && handler->write) {
                    if (n > w.size() - index) {
                        handler->write(ctx, WriteContract<C>{w, index, n});
                    }
                }
            }

            constexpr std::pair<bool, bool> direct_write(view::basic_rvec<C> t) {
                if (handler && handler->direct_write) {
                    return {true, handler->direct_write(ctx, t, 1)};
                }
                return {false, false};
            }

            constexpr std::pair<bool, bool> direct_write(C data, size_t n) {
                if (handler && handler->direct_write) {
                    if (handler->direct_buffer) {
                        view::basic_wvec<C> tmpbuf;
                        handler->direct_buffer(ctx, tmpbuf, n);
                        if (tmpbuf.size() == n) {
                            for (size_t i = 0; i < n; i++) {
                                tmpbuf[i] = data;
                            }
                            return {true, handler->direct_write(ctx, tmpbuf, n)};
                        }
                    }
                    view::basic_wvec<C> t{&data, 1};
                    return {true, handler->direct_write(ctx, t, n)};
                }
                return {false, false};
            }

            template <internal::ArrayReadableBuffer<C> T>
            constexpr std::pair<bool, bool> direct_write(T&& t) {
                if (handler && handler->direct_write) {
                    if (handler->direct_buffer) {
                        view::basic_wvec<C> tmpbuf;
                        handler->direct_buffer(ctx, tmpbuf, t.size());
                        if (tmpbuf.size() == t.size()) {
                            for (size_t i = 0; i < t.size(); i++) {
                                tmpbuf[i] = t[i];
                            }
                            return {true, handler->direct_write(ctx, tmpbuf, t.size())};
                        }
                    }
                    for (size_t i = 0; i < t.size(); i++) {
                        C data = t[i];
                        view::basic_wvec<C> tt{&data, 1};
                        if (!handler->direct_write(ctx, tt, 1)) {
                            return {true, false};
                        }
                    }
                    return {true, true};
                }
                return {false, false};
            }

            constexpr void offset_internal(size_t add) {
                if (w.size() - index < add) {
                    index = w.size();
                }
                else {
                    index += add;
                }
            }

            constexpr static void fill_null(basic_writer& o) {
                o.w = {};
                o.index = 0;
                o.handler = nullptr;
                o.ctx = nullptr;
            }

            // for clone
            constexpr basic_writer(view::basic_wvec<C> w, size_t index)
                : w(w), index(index) {}

            constexpr basic_writer(const WriteStreamHandler<C>* handler, void* ctx, view::basic_wvec<C> w, size_t index)
                : handler(handler), ctx(ctx), w(w), index(index) {}

           public:
            constexpr basic_writer(view::basic_wvec<C> w)
                : w(w) {}

            constexpr basic_writer(const WriteStreamHandler<C>* handler, void* ctx, view::basic_wvec<C> w = {})
                : handler(handler), ctx(ctx), w(w) {
                init();
            }

            template <class T>
            constexpr basic_writer(const WriteStreamHandlerT<C, T>* handler, typename WriteStreamHandlerT<C, T>::contract_type* ctx, view::basic_wvec<C> w = {})
                : handler(&handler->base), ctx(ctx), w(w) {
                init();
            }

            constexpr ~basic_writer() {
                destroy();
            }

            constexpr void* context() const noexcept {
                return ctx;
            }

            constexpr const WriteStreamHandler<C>* stream_handler() const noexcept {
                return handler;
            }

            constexpr basic_writer(const basic_writer& o) = delete;
            constexpr basic_writer(basic_writer&& o) noexcept
                : w(o.w),
                  index(o.index),
                  handler(o.handler),
                  ctx(o.ctx) {
                fill_null(o);
            }

            constexpr basic_writer& operator=(const basic_writer& o) = delete;
            constexpr basic_writer& operator=(basic_writer&& o) noexcept {
                if (this == &o) return *this;
                destroy();
                w = o.w;
                index = o.index;
                handler = o.handler;
                ctx = o.ctx;
                fill_null(o);
                return *this;
            }

            constexpr basic_writer clone() const {
                if (!handler) {
                    return basic_writer(w, index);
                }
                if (!handler->clone) {
                    // shallow copy
                    return basic_writer(handler, ctx, w, index);
                }
                void* new_ctx = nullptr;
                view::basic_wvec<C> new_w;
                handler->clone(ctx, w, new_ctx, new_w);
                return basic_writer(handler, new_ctx, new_w, index);
            }

            constexpr void reset(size_t pos = 0) {
                if (w.size() < pos) {
                    stream_write(pos - w.size());
                }
                if (w.size() < pos) {
                    index = w.size();
                    return;
                }
                index = pos;
            }

            constexpr void reset_buffer(view::basic_wvec<C> o, size_t pos = 0) {
                destroy();
                w = o;
                handler = nullptr;
                ctx = nullptr;
                reset(pos);
            }

            constexpr void reset_buffer(const WriteStreamHandler<C>* h, void* c, view::basic_wvec<C> o = {}, size_t pos = 0) {
                destroy();
                w = o;
                handler = h;
                ctx = c;
                init();
                reset(pos);
            }

            constexpr bool write(C data, size_t n) {
                stream_write(n);
                if (auto [called, ok] = direct_write(data, n); called) {
                    if (ok) {
                        offset_internal(n);
                    }
                    return ok;
                }
                auto rem = w.substr(index, n);
                for (auto& d : rem) {
                    d = data;
                }
                index += rem.size();
                return rem.size() == n;
            }

            constexpr bool write(view::basic_rvec<C> t) {
                stream_write(t.size());
                if (auto [called, ok] = direct_write(t); called) {
                    if (ok) {
                        offset_internal(t.size());
                    }
                    return ok;
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                auto res = copy_(w.substr(index), t);
                if (res < 0) {
                    index = w.size();
                    return false;
                }
                else {
                    index += t.size();
                    return true;
                }
            }

            template <internal::ArrayReadableBuffer<C> T>
            constexpr bool write(T&& t) {
                stream_write(t.size());
                if (auto [called, ok] = direct_write(t); called) {
                    if (ok) {
                        offset_internal(t.size());
                    }
                    return ok;
                }
                auto to_write = w.substr(index, t.size());
                for (size_t i = 0; i < to_write.size(); i++) {
                    to_write[i] = t[i];
                }
                index += to_write.size();
                return to_write.size() == t.size();
            }

            constexpr basic_writer& push_back(C add) {
                stream_write(1);
                if (auto [called, ok] = direct_write(add, 1); called) {
                    if (ok) {
                        offset_internal(1);
                    }
                    return *this;
                }
                auto sub = w.substr(index);
                if (sub.empty()) {
                    return *this;
                }
                sub[0] = add;
                offset_internal(1);
                return *this;
            }

            template <class T>
            constexpr basic_writer& append(T&& t)
                requires std::is_same_v<std::decay_t<T>, C> || std::is_convertible_v<T, view::basic_rvec<C>> || internal::ArrayReadableBuffer<T, C>
            {
                using decayT = std::decay_t<T>;
                if constexpr (std::is_same_v<decayT, C>) {
                    push_back(t);
                }
                else if constexpr (std::is_convertible_v<T, view::basic_rvec<C>>) {
                    write(t);
                }
                else if constexpr (internal::ArrayReadableBuffer<T, C>) {
                    write(std::forward<T>(t));
                }
                else {
                    static_assert(std::is_same_v<decayT, C>, "unsupported type");
                }
                return *this;
            }

            template <class T>
            constexpr basic_writer& operator<<(T&& t)
                requires std::is_same_v<std::decay_t<T>, C> || std::is_convertible_v<T, view::basic_rvec<C>> || internal::ArrayReadableBuffer<T, C>
            {
                return append(std::forward<T>(t));
            }

            constexpr bool full() const {
                return w.size() == index && (!handler || handler->full(this->ctx, index));
            }

            // remain returns the remaining space in the buffer
            // this function does not operate on the write stream
            constexpr view::basic_wvec<C> remain() const noexcept {
                return w.substr(index);
            }

            constexpr void offset(size_t add) {
                stream_write(add);
                offset_internal(add);
            }

            constexpr size_t offset() const {
                return index;
            }

            // written() returns the written part of the buffer
            constexpr view::basic_wvec<C> written() const noexcept {
                return w.substr(0, index);
            }

            // commit commits all written bytes to the write stream
            constexpr bool commit() {
                return commit(index);
            }

            // commit commits n bytes to the write stream
            // returns true if the commit called but not guaranteed
            // that the commit is successful
            constexpr bool commit(size_t n) {
                if (!handler || handler->commit == nullptr || n > index) {
                    return false;
                }
                handler->commit(ctx, CommitContract<C>{w, index, n});
                return true;
            }

            // prepare_stream checks if the buffer has enough space to write n bytes
            constexpr bool prepare_stream(size_t n) {
                stream_write(n);
                return remain().size() >= n;
            }

            // is_stream returns true if the write stream is active
            constexpr bool is_stream() const {
                return handler != nullptr;
            }

            // is_direct_stream returns true if the write stream is direct
            constexpr bool is_direct_stream() const {
                return handler && handler->direct_write;
            }
        };

        using writer = basic_writer<byte>;

        namespace internal {
            template <class C, ResizableBuffer<C> T>
            struct ResizableBufferWriteStream {
                constexpr static bool full(void* ctx, size_t offset) {
                    auto self = static_cast<T*>(ctx);
                    if constexpr (internal::has_max_size<T>) {
                        return offset == self->max_size();
                    }
                    return false;
                }

                constexpr static void write(void* ctx, WriteContract<C> c) {
                    auto self = static_cast<T*>(ctx);
                    self->resize(c.least_new_buffer_size());
                    c.set_new_buffer(*self);
                }

                constexpr static void commit(void* ctx, CommitContract<C> c) {
                    auto self = static_cast<T*>(ctx);
                    c.direct_drop(c.require_drop());
                    self->resize(c.buffer().size());
                    c.replace_buffer(*self);
                }

                constexpr static WriteStreamHandler<C> handler = {
                    .full = full,
                    .write = write,
                    .commit = commit,
                };
            };
        }  // namespace internal

        template <class T, class C = byte>
        const WriteStreamHandler<C>* resizable_buffer_writer() {
            return &internal::ResizableBufferWriteStream<C, T>::handler;
        }

        template <class T, class C = byte>
        struct WriteStreamingBuffer {
           private:
            T buffer;
            size_t offset = 0;

           public:
            constexpr T& get_buffer() noexcept {
                return buffer;
            }

            constexpr T take_buffer() noexcept {
                offset = 0;
                return std::move(buffer);
            }

            constexpr size_t size() const noexcept {
                return buffer.size();
            }

            constexpr auto stream(auto&& cb) {
                basic_writer<C> w{resizable_buffer_writer<T>(), &buffer, buffer};
                w.reset(offset);
                // exception safe
                // to prevent include <helper/defer.h>
                struct L {
                    basic_writer<C>* w;
                    size_t* offset;
                    L(size_t* offset, basic_writer<C>* w)
                        : offset(offset), w(w) {}
                    ~L() {
                        *offset = w->offset();
                    }
                } l{&offset, &w};
                return cb(w);
            }
        };

        namespace test {
            struct ArrayStreamWriter {
                static constexpr auto N = 100;
                byte buffer[N]{};
                size_t used = 0;

                constexpr static bool full(void* ctx, size_t offset) {
                    auto self = static_cast<ArrayStreamWriter*>(ctx);
                    return offset == N;
                }

                constexpr static void write(void* ctx, WriteContract<byte> c) {
                    auto len = c.least_new_buffer_size();
                    if (len > N) {
                        len = N;
                    }
                    auto self = static_cast<ArrayStreamWriter*>(ctx);
                    c.set_new_buffer(view::basic_wvec<byte>(self->buffer, len));
                }

                static constexpr WriteStreamHandler<byte> handler = {
                    .full = full,
                    .write = write,
                };
            };

            inline bool test_write_stream() {
                ArrayStreamWriter stream;
                binary::writer w(&ArrayStreamWriter::handler, &stream);
                if (!w.remain().empty()) {
                    throw "unexpected remain";
                }
                if (!w.write(0x01, 1)) {
                    throw "write failed";
                }
                if (!w.remain().empty()) {
                    throw "unexpected remain";
                }
                if (!w.prepare_stream(10)) {
                    throw "prepare_stream failed";
                }
                if (!w.write("hello")) {
                    throw "write failed";
                }
                if (w.remain().size() != 5) {
                    throw "unexpected remain";
                }
                if (w.written() != "\x01hello") {
                    throw "unexpected written";
                }
                return true;
            }

        }  // namespace test
    }  // namespace binary
}  // namespace futils
