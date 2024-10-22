/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/reader.h>
#include <binary/writer.h>
#include "file.h"

namespace futils::file {

    namespace internal {
        template <class F>
        concept as_file =
            std::is_same_v<F, File> || std::is_same_v<F, File&> ||
            std::is_same_v<F, const File&>;

        static_assert(as_file<File> && as_file<File&>);

        template <class L>
        concept locker = requires(L l) {
            { l.lock() };
            { l.unlock() };
        };

        struct empty_lock {
            void lock() const {}
            void unlock() const {}
        };

    }  // namespace internal

    template <class B, internal::as_file F = const File&, internal::locker L = internal::empty_lock>
    struct FileStream {
        F file;
        L lock;
        B buffer;
        FileError error;
        bool eof = false;
        size_t last_read = 0;
        size_t last_expected = 0;

       private:
        static bool file_empty(void* ctx, size_t index) {
            auto self = static_cast<FileStream*>(ctx);
            const File& f = self->file;
            auto s = f.stat();
            if (!s) {
                self->error = s.error();
                return true;
            }
            if (s->mode.pipe() || s->mode.terminal() || s->mode.socket()) {
                return self->eof;
            }
            return self->eof || index >= s->size;
        }

        static void file_read(void* ctx, binary::ReadContract<byte> c) {
            auto req = c.least_requested();
            auto cur = c.buffer().size();
            auto next = c.least_new_buffer_size();
            auto self = static_cast<FileStream*>(ctx);
            const File& f = self->file;
            self->buffer.resize(next);
            auto buffer = view::wvec(self->buffer).substr(cur, req);
            self->last_expected = req;
            self->last_read = 0;
            self->lock.lock();
            auto d = helper::defer([&] { self->lock.unlock(); });
            while (buffer.size() > 0) {
                auto read = f.read_file(buffer);
                if (!read || read->empty()) {
                    self->eof = true;
                    if (!read) {
                        self->error = read.error();
                        return;
                    }
                    break;
                }
                self->last_read += read->size();
                buffer = buffer.substr(read->size());
            }
            d.execute();
            self->buffer.resize(cur + self->last_read);
            c.set_new_buffer(self->buffer);
        }

        static void increase_buffer(void* ctx, binary::WriteContract<byte> c) {
            auto self = static_cast<FileStream*>(ctx);
            auto req = c.least_requested();
            auto cur = c.buffer().size();
            auto next = c.least_new_buffer_size();
            self->buffer.resize(next);
            c.set_new_buffer(self->buffer);
        }

        static void file_commit(void* ctx, binary::CommitContract<byte> c) {
            auto self = static_cast<FileStream*>(ctx);
            if (self->eof) return;
            auto buf = c.buffer();
            auto to_commit = buf.rsubstr(0, c.require_drop());
            self->lock.lock();
            auto d = helper::defer([&] { self->lock.unlock(); });
            auto res = self->file.write_file_all(to_commit);
            d.execute();
            if (!res) {
                self->error = res.error();
                self->eof = true;
                return;
            }
            size_t total_written = buf.size() - to_commit.size();
            if (buf.size() == total_written) {
                c.set_new_buffer(self->buffer, total_written);
                return;
            }
            c.direct_drop(total_written);
            self->buffer.resize(c.buffer().size());
            c.replace_buffer(self->buffer);
        }

        static bool direct_write(void* ctx, view::basic_rvec<byte> w, size_t count) {
            auto self = static_cast<FileStream*>(ctx);
            self->lock.lock();
            auto d = helper::defer([&] { self->lock.unlock(); });
            for (size_t i = 0; i < count; i++) {
                auto res = self->file.write_file_all(w);
                if (!res) {
                    self->error = res.error();
                    self->eof = true;
                    return false;
                }
            }
            d.execute();
            return true;
        }

        static void direct_buffer(void* ctx, view::basic_wvec<byte>& w, size_t expected) {
            auto self = static_cast<FileStream*>(ctx);
            self->buffer.resize(expected);
            w = view::basic_wvec<byte>(self->buffer);
        }

        static void finalize(void* ctx, binary::CommitContract<byte> c) {
            file_commit(ctx, c);
        }

        static void discard_buffer(void* ctx, binary::DiscardContract<byte> c) {
            auto self = static_cast<FileStream*>(ctx);
            c.direct_drop(c.require_drop());
            self->buffer.resize(c.buffer().size());
            c.replace_buffer(self->buffer);
        }

        static constexpr binary::ReadStreamHandler<byte> read_handler = {
            .empty = file_empty,
            .read = file_read,
            .discard = discard_buffer,
        };

        static constexpr binary::WriteStreamHandler<byte> write_handler = {
            .full = [](void* self, size_t) {
                auto fs = static_cast<FileStream*>(self);
                return fs->eof;
            },
            .write = increase_buffer,
            .commit = file_commit,
            .destroy = finalize,
        };

        static constexpr binary::WriteStreamHandler<byte> direct_write_handler = {
            .full = [](void* self, size_t) {
                auto fs = static_cast<FileStream*>(self);
                return fs->eof;
            },
            .direct_write = direct_write,
            .direct_buffer = direct_buffer,
        };

       public:
        static auto get_read_handler() {
            return &read_handler;
        }

        static auto get_write_handler() {
            return &write_handler;
        }

        static auto get_direct_write_handler() {
            return &direct_write_handler;
        }
    };
}  // namespace futils::file
